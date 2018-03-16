// Fill out your copyright notice in the Description page of Project Settings.

#include "CameraCaptureComponent.h"
#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Engine/Texture2D.h"
#include "Containers/Queue.h"
#include "Containers/Array.h"
#include "Lumin/LuminPlatformFile.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "IMessageBus.h"
#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"

#include <ml_camera.h>
#include <ml_camera_metadata.h>
#include <ml_camera_metadata_tags.h>

DEFINE_LOG_CATEGORY(LogCameraCapture);

enum CaptureMsgType
{
  Request,
  Response,
  Log
};

struct FCaptureMessage
{
  CaptureMsgType Type;
  MLCameraCaptureType CaptureType;
  FString Log;
  FString FilePath;
  bool Success;
  UTexture2D* Texture;
  float Duration;

  FCaptureMessage()
    : Type(CaptureMsgType::Request)
    , CaptureType(MLCameraCaptureType_Image)
    , Log("")
    , Success(false)
    , Texture(nullptr)
    , Duration(0.0f)
  {}
};

class FImageCaptureImpl : public FRunnable
{
public:
  FImageCaptureImpl()
    : Thread(nullptr)
    , StopTaskCounter(0)
    , Semaphore(nullptr)
    , CameraOutput(nullptr)
    , ImgExtension(".jpeg")
    , VidExtension(".mp4")
	, bCameraConnected(false)
	, RetryConnectWaitTime(0.5f)
  {
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);	

    Semaphore = FGenericPlatformProcess::GetSynchEventFromPool(false);
    Thread = FRunnableThread::Create(this, TEXT("FCameraCaptureWorker"), 0, TPri_BelowNormal);
  }

  ~FImageCaptureImpl()
  {
    StopTaskCounter.Increment();
    Semaphore->Trigger();
    Thread->WaitForCompletion();

    for (auto& CapturedTexture : CapturedTextures)
    {
      CapturedTexture->RemoveFromRoot();
    }

    if (Semaphore)
    {
      FGenericPlatformProcess::ReturnSynchEventToPool(Semaphore);
      Semaphore = nullptr;
    }

    delete Thread;
    Thread = nullptr;

	if (bCameraConnected)
	{
		MLCameraDisconnect();
	}
  }

  virtual uint32 Run() override
  {
	  while (StopTaskCounter.GetValue() == 0)
	  {
		  if (!bCameraConnected)
		  {
			  bCameraConnected = MLCameraConnect();
			  if (!bCameraConnected)
			  {
				  Log(FString::Printf(TEXT("Camera connection attempt failed!  Retrying in %.2f seconds"), RetryConnectWaitTime));
				  IncomingMessages.Empty();
				  FPlatformProcess::Sleep(RetryConnectWaitTime);
			  }
		  }
		  else
		  {
			  if (IncomingMessages.Dequeue(CurrentMessage))
			  {
				  BeginCapture();
			  }

			  Semaphore->Wait();
		  }
	  }

	  return 0;
  }

  void BeginCapture()
  {
    MLHandle Handle = MLCameraPrepareCapture(CurrentMessage.CaptureType);

    if (Handle == ML_INVALID_HANDLE)
    {
      Log(TEXT("MLCameraPrepareCapture failed!  Camera capture aborted!"));
      EndCapture(false);
      return;
    }

    switch (CurrentMessage.CaptureType)
    {
    case MLCameraCaptureType_Image:
    {
      Log(TEXT("Beginning capture image to file."));

#if PLATFORM_LUMIN
      IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
      // This module is only for Lumin so this is fine for now.
      FLuminPlatformFile* LuminPlatformFile = static_cast<FLuminPlatformFile*>(&PlatformFile);
      UniqueFileName = LuminPlatformFile->ConvertToLuminPath(FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("Img_"), *ImgExtension), true);
#endif
      if (!MLCameraCaptureImage(TCHAR_TO_UTF8(*UniqueFileName)))
      {
        Log(TEXT("MLCameraCaptureImage failed!  Camera capture aborted!"));
        EndCapture(false);
        return;
      }

      EndCapture(true);
    }
    break;

    case MLCameraCaptureType_ImageRaw:
    {
      CameraOutput = nullptr;

      Log(TEXT("Beginning capture image to texture."));

      if (!MLCameraCaptureImageRaw())
      {
        Log(TEXT("MLCameraCaptureImageRaw failed!  Camera capture aborted!"));
        EndCapture(false);
        return;
      }

      if (!MLCameraGetImageStream(&CameraOutput))
      {
        Log(TEXT("MLCameraGetImageStream failed!  Camera capture aborted!"));
        EndCapture(false);
        return;
      }

      if (CameraOutput->plane_count == 0)
      {
        Log(TEXT("Invalid plane_count!  Camera capture aborted!"));
        EndCapture(false);
        return;
      }

      EndCapture(true);
    }
    break;

    case MLCameraCaptureType_Video:
    {
      Log(TEXT("Beginning capture video to file."));

#if PLATFORM_LUMIN
      IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
      // This module is only for Lumin so this is fine for now.
      FLuminPlatformFile* LuminPlatformFile = static_cast<FLuminPlatformFile*>(&PlatformFile);
      UniqueFileName = LuminPlatformFile->ConvertToLuminPath(FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("Vid_"), *VidExtension), true);
#endif
      if (!MLCameraCaptureVideoStart(TCHAR_TO_UTF8(*UniqueFileName)))
      {
        Log(TEXT("MLCameraCaptureVideoStart failed!  Video capture aborted!"));
        EndCapture(false);
        return;
      }

      FPlatformProcess::Sleep(CurrentMessage.Duration);
      EndCapture(true);
    }
    break;
  }
  }

  void EndCapture(bool InSuccess)
  {
    CurrentMessage.Type = CaptureMsgType::Response;
    CurrentMessage.Success = InSuccess;
	CurrentMessage.FilePath = UniqueFileName;

    if (!InSuccess)
    {
      uint32 DeviceStatus = 0;
      if (MLCameraGetDeviceStatus(&DeviceStatus))
      {
        if ((MLCameraDeviceStatusFlag_Available & DeviceStatus) != 0)
        {
          Log(TEXT("Device status = MLCameraDeviceStatusFlag_Available"));
        }
        else if ((MLCameraDeviceStatusFlag_Opened & DeviceStatus) != 0)
        {
          Log(TEXT("Device status = MLCameraDeviceStatusFlag_Opened"));
        }
        else if ((MLCameraDeviceStatusFlag_Disconnected  & DeviceStatus) != 0)
        {
          Log(TEXT("Device status = MLCameraDeviceStatusFlag_Disconnected "));
        }
        else if ((MLCameraDeviceStatusFlag_Error & DeviceStatus) != 0)
        {
          Log(TEXT("Device status = MLCameraDeviceStatusFlag_Error"));
          MLCameraError CameraError;
          if (MLCameraGetErrorCode(&CameraError))
          {
            switch (CameraError)
            {
            case MLCameraError_None: Log(TEXT("Error = MLCameraError_None")); break;
            case MLCameraError_Invalid: Log(TEXT("Error = MLCameraError_Invalid")); break;
            case MLCameraError_Disabled: Log(TEXT("Error = MLCameraError_Disabled")); break;
            case MLCameraError_DeviceFailed: Log(TEXT("Error = MLCameraError_DeviceFailed")); break;
            case MLCameraError_ServiceFailed: Log(TEXT("Error = MLCameraError_ServiceFailed")); break;
            case MLCameraError_CaptureFailed: Log(TEXT("Error = MLCameraError_CaptureFailed")); break;
            case MLCameraError_Unknown: Log(TEXT("Error = MLCameraError_Unknown")); break;
            }
          }
          else
          {
            Log(TEXT("MLCameraGetErrorCode failed!"));
          }
        }
      }
      else
      {
        Log(TEXT("MLCameraGetDeviceStatus failed!"));
      }
    }

    switch (CurrentMessage.CaptureType)
    {
    case MLCameraCaptureType_Image:
    {
      if (InSuccess)
      {
        Log(FString::Printf(TEXT("Captured image to %s"), *UniqueFileName));
      }
    }
    break;

    case MLCameraCaptureType_ImageRaw:
    {
      if (InSuccess)
      {
        MLCameraPlaneInfo& ImageInfo = CameraOutput->planes[0];
        //JMC: ImageInfo.width/height are incorrect, ImageInfo.data contains header info also as at MLSKD 0.9.0
        // using IImageWrapperModule as workaround/potentially permanent solution
        if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(ImageInfo.data, ImageInfo.size))
        {
          const TArray<uint8>* RawData = NULL;
          if (ImageWrapper->GetRaw(ImageWrapper->GetFormat(), 8, RawData))
          {
            Log(FString::Printf(TEXT("ImageWrapper width=%d height=%d size=%d"), ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), RawData->Num()));
            UTexture2D* CaptureTexture = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), EPixelFormat::PF_B8G8R8A8);
            CaptureTexture->AddToRoot();
            CapturedTextures.Add(CaptureTexture);
            FTexture2DMipMap& Mip = CaptureTexture->PlatformData->Mips[0];
            void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
            FMemory::Memcpy(Data, RawData->GetData(), Mip.BulkData.GetBulkDataSize());
            Mip.BulkData.Unlock();
            CaptureTexture->UpdateResource();
            CurrentMessage.Texture = CaptureTexture;
          }
        }
      }     
    }
    break;

    case MLCameraCaptureType_Video:
    {
      if (InSuccess)
      {
        MLCameraCaptureVideoStop();
        Log(FString::Printf(TEXT("Captured video to %s"), *UniqueFileName));
      }
    }
    break;
    }

	// clear any left over events from this capture session
	IncomingMessages.Empty();
	// signal the update thread to handle the capture result
    OutgoingMessages.Enqueue(CurrentMessage);
  }

  void Log(const FString& Info)
  {
    FCaptureMessage Msg;
    Msg.Type = CaptureMsgType::Log;
    Msg.Log = Info;
    OutgoingMessages.Enqueue(Msg);
  }

  void ProcessCaptureMessage(const FCaptureMessage& InMsg)
  {
	  if (bCameraConnected)
	  {
		  IncomingMessages.Enqueue(InMsg);
		  // wake up the worker to process the event
		  Semaphore->Trigger();
	  }
	  else
	  {
		  Log(TEXT("Discarding capture task due to lack of camera connection."));
	  }
  }

  /** Internal thread this instance is running on */
  FRunnableThread* Thread;
  FThreadSafeCounter StopTaskCounter;
  TQueue<FCaptureMessage, EQueueMode::Spsc> IncomingMessages;
  TQueue<FCaptureMessage, EQueueMode::Spsc> OutgoingMessages;
  FEvent* Semaphore;
  FCaptureMessage CurrentMessage;
  bool bCameraConnected;
  const float RetryConnectWaitTime;

  MLCameraDeviceStatusCallbacks DeviceStatusCallbacks;
  MLCameraCaptureCallbacks CaptureCallbacks;
  MLCameraOutput* CameraOutput;

  const FString ImgExtension;
  const FString VidExtension;
  FString UniqueFileName;
  TArray<UTexture2D*> CapturedTextures;
  TSharedPtr<IImageWrapper> ImageWrapper;
};

UCameraCaptureComponent::UCameraCaptureComponent()
    : Impl(nullptr)
    , bCapturing(false)
{
  PrimaryComponentTick.TickGroup = TG_PrePhysics;
  PrimaryComponentTick.bStartWithTickEnabled = true;
  PrimaryComponentTick.bCanEverTick = true;
}

UCameraCaptureComponent::~UCameraCaptureComponent()
{
  delete Impl;
  Impl = nullptr;
}

void UCameraCaptureComponent::BeginPlay()
{
  Super::BeginPlay();
  Impl = new FImageCaptureImpl;
}

void UCameraCaptureComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

  if (!Impl->OutgoingMessages.IsEmpty())
  {
    FCaptureMessage Msg;
    Impl->OutgoingMessages.Dequeue(Msg);

    if (Msg.Type == CaptureMsgType::Request)
    {
      Log("Unexpected CaptureMsgType::Request received from worker thread!");
    }
    else if (Msg.Type == CaptureMsgType::Log)
    {
      Log(Msg.Log);
    }
    else if (Msg.Type == CaptureMsgType::Response)
    {
      switch (Msg.CaptureType)
      {
      case MLCameraCaptureType_Image:
      {
        if (Msg.Success)
        {
          CaptureImgToFileSuccess.Broadcast(Msg.FilePath);
        }
        else
        {
          CaptureImgToFileFailure.Broadcast();
        }
      }
      break;

      case MLCameraCaptureType_ImageRaw:
      {
        if (Msg.Success)
        {
          CaptureImgToTextureSuccess.Broadcast(Msg.Texture);
        }
        else
        {
          CaptureImgToTextureFailure.Broadcast();
        }
      }
      break;

      case MLCameraCaptureType_Video:
      {
        if (Msg.Success)
        {
          CaptureVidToFileSuccess.Broadcast(Msg.FilePath);
        }
        else
        {
          CaptureVidToFileFailure.Broadcast();
        }
      }
      break;
      }

      bCapturing = false;
    }
  }
}

UCameraCaptureComponent::FCameraCaptureLogMessage& UCameraCaptureComponent::OnCaptureLogMessage()
{
  return CaptureLogMessage;
}

UCameraCaptureComponent::FCameraCaptureImgToFileSuccess& UCameraCaptureComponent::OnCaptureImgToFileSuccess()
{
  return CaptureImgToFileSuccess;
}

UCameraCaptureComponent::FCameraCaptureImgToFileFailure& UCameraCaptureComponent::OnCaptureImgToFileFailure()
{
  return CaptureImgToFileFailure;
}

UCameraCaptureComponent::FCameraCaptureImgToTextureSuccess& UCameraCaptureComponent::OnCaptureImgToTextureSuccess()
{
  return CaptureImgToTextureSuccess;
}

UCameraCaptureComponent::FCameraCaptureImgToTextureFailure& UCameraCaptureComponent::OnCaptureImgToTextureFailure()
{
  return CaptureImgToTextureFailure;
}

UCameraCaptureComponent::FCameraCaptureVidToFileSuccess& UCameraCaptureComponent::OnCaptureVidToFileSuccess()
{
  return CaptureVidToFileSuccess;
}

UCameraCaptureComponent::FCameraCaptureVidToFileFailure& UCameraCaptureComponent::OnCaptureVidToFileFailure()
{
  return CaptureVidToFileFailure;
}

bool UCameraCaptureComponent::CaptureImageToFileAsync()
{
  if (!bCapturing)
  {
    bCapturing = true;
    FCaptureMessage Msg;
    Msg.Type = CaptureMsgType::Request;
    Msg.CaptureType = MLCameraCaptureType_Image;
    Impl->ProcessCaptureMessage(Msg);
    return true;
  }

  Log("Camera capture already in progress!");
  return false;
}

bool UCameraCaptureComponent::CaptureImageToTextureAsync()
{
  if (!bCapturing)
  {
    bCapturing = true;
    FCaptureMessage Msg;
    Msg.Type = CaptureMsgType::Request;
    Msg.CaptureType = MLCameraCaptureType_ImageRaw;
    Impl->ProcessCaptureMessage(Msg);
    return true;
  }

  Log("Camera capture already in progress!");
  return false;
}

bool UCameraCaptureComponent::CaptureVideoToFileAsync(float InDuration)
{
  if (!bCapturing)
  {
    bCapturing = true;
    FCaptureMessage Msg;
    Msg.Type = CaptureMsgType::Request;
    Msg.CaptureType = MLCameraCaptureType_Video;
    Msg.Duration = InDuration;
    Impl->ProcessCaptureMessage(Msg);
    return true;
  }

  Log("Camera capture already in progress!");
  return false;
}

void UCameraCaptureComponent::Log(const FString& LogMessage)
{
  UE_LOG(LogCameraCapture, Log, TEXT("%s"), *LogMessage);
  CaptureLogMessage.Broadcast(LogMessage);
}
