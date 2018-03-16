// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

#include "ImageTrackerComponent.h"
#include "MagicLeapHMD.h"
#include "AppFramework.h"
#include "MagicLeapMath.h"
#include "Runtime/Core/Public/HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "AppEventHandler.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

#include <ml_image_tracking.h>
#include <ml_perception.h>
#include <ml_snapshot.h>
#include <ml_head_tracking.h>
#include <ml_coordinate_frame_uid.h>

class FImageTrackerEngineInterface : public MagicLeap::IAppEventHandler
{
public:
	static TWeakPtr<FImageTrackerEngineInterface, ESPMode::ThreadSafe> Get()
	{
		if (!Instance.IsValid())
		{
			Instance = MakeShareable(new FImageTrackerEngineInterface());
		}

		return Instance;
	}

	MLHandle GetHandle() const
	{
		return ImageTracker;
	}

private:
	FImageTrackerEngineInterface()
	: ImageTracker(ML_INVALID_HANDLE)
	{
		UE_LOG(LogMagicLeap, Display, TEXT("[FImageTrackerEngineInterface] Creating Image Tracker"));
		FMemory::Memset(&Settings, 0, sizeof(Settings));
		bool bResult = MLImageTrackerInitSettings(&Settings);

		if (!bResult)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerEngineInterface] Could not initialize image tracker settings mofo"));
		}

		ImageTracker = MLImageTrackerCreate(&Settings);

		if (!MLHandleIsValid(ImageTracker))
		{
			UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerEngineInterface] Could not create Image tracker."));
		}
	}	

	void OnAppPause() override
	{
		bWasSystemEnabledOnPause = Settings.enable_image_tracking;

		if (!bWasSystemEnabledOnPause)
		{
			UE_LOG(LogMagicLeap, Log, TEXT("[FImageTrackerEngineInterface] Image tracking was not enabled at time of application pause."));
		}
		else
		{
			if (!MLHandleIsValid(ImageTracker))
			{
				UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerEngineInterface] Image tracker was invalid on application pause."));
			}
			else
			{
				Settings.enable_image_tracking = false;

				if (!MLImageTrackerUpdateSettings(ImageTracker, &Settings))
				{
					UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerEngineInterface] Failed to disable image tracker on application pause."));
				}
				else
				{
					UE_LOG(LogMagicLeap, Log, TEXT("[FImageTrackerEngineInterface] Image tracker paused until app resumes."));
				}
			}
		}
	}

	void OnAppResume() override
	{
		if (!bWasSystemEnabledOnPause)
		{
			UE_LOG(LogMagicLeap, Log, TEXT("[FImageTrackerEngineInterface] Not resuming image tracker as it was not enabled at time of application pause."));
		}
		else
		{
			if (!MLHandleIsValid(ImageTracker))
			{
				UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerEngineInterface] Image tracker was invalid on application resume."));
			}
			else
			{
				Settings.enable_image_tracking = true;

				if (!MLImageTrackerUpdateSettings(ImageTracker, &Settings))
				{
					UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerEngineInterface] Failed to re-enable image tracker on application resume."));
				}
				else
				{
					UE_LOG(LogMagicLeap, Log, TEXT("[FImageTrackerEngineInterface] Image tracker re-enabled on application resume."));
				}
			}
		}
	}

	void OnAppShutDown() override
	{
		if (MLHandleIsValid(ImageTracker))
		{
			bool bResult = MLImageTrackerDestroy(ImageTracker);

			if (!bResult)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerEngineInterface] Error destroying image tracker."));
			}

			ImageTracker = ML_INVALID_HANDLE;
		}
	}

	MLHandle ImageTracker;
	MLImageTrackerSettings Settings;
	static TSharedPtr<FImageTrackerEngineInterface, ESPMode::ThreadSafe> Instance;
};

TSharedPtr<FImageTrackerEngineInterface, ESPMode::ThreadSafe> FImageTrackerEngineInterface::Instance;

struct FTrackerMessage
{
	enum TaskType
	{
		None,
		Create,
		ReportStatus,
	};

	TaskType TaskType;
	FString TargetName;
	MLImageTrackerTargetSettings TargetSettings;
	UTexture2D* TargetImageTexture;
	MLImageTrackerTargetResult TrackingStatus;

	FTrackerMessage()
	: TaskType(TaskType::None)
	{}
};

class FImageTrackerImpl : public FRunnable
{
public:
	FImageTrackerImpl()
		: Target(ML_INVALID_HANDLE)
#if WITH_EDITOR
		, TextureBeforeEdit(nullptr)
#endif
		, Thread(nullptr)
		, StopTaskCounter(0)
		, bIsTracking(false)
		, bHasTarget(false)
	{
		OldTrackingStatus.status = MLImageTrackerTargetStatus_Ensure32Bits;
		Thread = FRunnableThread::Create(this, TEXT("ImageTrackerWorker"), 0, TPri_BelowNormal);
	};

	virtual ~FImageTrackerImpl()
	{
		StopTaskCounter.Increment();

		if (nullptr != Thread)
		{
			Thread->WaitForCompletion();
			delete Thread;
			Thread = nullptr;
		}

		if (MLHandleIsValid(Target))
		{
			checkf(ImageTracker.Pin().IsValid(), TEXT("[FImageTrackerImpl] ImageTracker weak pointer is invalid!"));
			MLImageTrackerRemoveTarget(ImageTracker.Pin()->GetHandle(), Target);
			Target = ML_INVALID_HANDLE;
		}

		ImageTracker.Reset();
	}

	virtual uint32 Run() override
	{
		while (StopTaskCounter.GetValue() == 0)
		{
			if (IncomingMessages.Dequeue(CurrentMessage))
			{
				DoTasks();
			}

			if (bIsTracking)
			{
				FTrackerMessage TrackingStatusMsg;
				TrackingStatusMsg.TaskType = FTrackerMessage::TaskType::ReportStatus;
				checkf(ImageTracker.Pin().IsValid(), TEXT("[FImageTrackerImpl] ImageTracker weak pointer is invalid!"));
				if (!MLImageTrackerGetTargetResult(ImageTracker.Pin()->GetHandle(), Target, &TrackingStatusMsg.TrackingStatus))
				{
					TrackingStatusMsg.TrackingStatus.status = MLImageTrackerTargetStatus_NotTracked;
				}

				OutgoingMessages.Enqueue(TrackingStatusMsg);
			}

			FPlatformProcess::Sleep(0.5f);
		}

		return 0;
	}

	void SetTargetAsync(const FString& InName, bool bInIsStationary, float InLongerDimension, UTexture2D* InTargetTexture)
	{
		if (!ImageTracker.IsValid())
		{
			ImageTracker = FImageTrackerEngineInterface::Get();
		}

		bHasTarget = true;
		MLImageTrackerTargetSettings TargetSettings;
		TargetSettings.longer_dimension = InLongerDimension;
		TargetSettings.is_stationary = bInIsStationary;
		FTrackerMessage CreateTargetMsg;
		CreateTargetMsg.TaskType = FTrackerMessage::TaskType::Create;
		CreateTargetMsg.TargetName = InName;
		CreateTargetMsg.TargetSettings = TargetSettings;
		CreateTargetMsg.TargetImageTexture = InTargetTexture;
		IncomingMessages.Enqueue(CreateTargetMsg);
	}
	
public:
	MLHandle Target;
	MLImageTrackerTargetStaticData Data;
	MLImageTrackerTargetResult OldTrackingStatus;
#if WITH_EDITOR
	UTexture2D* TextureBeforeEdit;
#endif
	TQueue<FTrackerMessage, EQueueMode::Spsc> OutgoingMessages;
	bool bHasTarget;

private:
	TWeakPtr<FImageTrackerEngineInterface, ESPMode::ThreadSafe> ImageTracker;
	FRunnableThread* Thread;
	FThreadSafeCounter StopTaskCounter;
	TQueue<FTrackerMessage, EQueueMode::Spsc> IncomingMessages;
	FTrackerMessage CurrentMessage;
	bool bIsTracking;

	void DoTasks()
	{
		switch (CurrentMessage.TaskType)
		{
		case FTrackerMessage::TaskType::None: break;
		case FTrackerMessage::TaskType::Create: SetTarget(); break;
		case FTrackerMessage::TaskType::ReportStatus: break;
		}
	}

	void SetTarget()
	{
		if (!MLHandleIsValid(Target))
		{
			CurrentMessage.TargetSettings.name = TCHAR_TO_UTF8(*CurrentMessage.TargetName);
			FTexture2DMipMap& Mip = CurrentMessage.TargetImageTexture->PlatformData->Mips[0];
			const unsigned char* PixelData = static_cast<const unsigned char*>(Mip.BulkData.Lock(LOCK_READ_ONLY));
			checkf(ImageTracker.Pin().IsValid(), TEXT("[FImageTrackerImpl] ImageTracker weak pointer is invalid!"));
			Target = MLImageTrackerAddTargetFromArray(ImageTracker.Pin()->GetHandle(), &CurrentMessage.TargetSettings, PixelData, CurrentMessage.TargetImageTexture->GetSurfaceWidth(), CurrentMessage.TargetImageTexture->GetSurfaceHeight(), MLImageTrackerImageFormat_RGBA);
			Mip.BulkData.Unlock();

			if (!MLHandleIsValid(Target))
			{
				UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerImpl] Could not create Image Target."));
				return;
			}

			// [3] Cache all the static data for this target.
			checkf(ImageTracker.Pin().IsValid(), TEXT("[FImageTrackerImpl] ImageTracker weak pointer is invalid!"));
			if (!MLImageTrackerGetTargetStaticData(ImageTracker.Pin()->GetHandle(), Target, &Data))
			{
				UE_LOG(LogMagicLeap, Error, TEXT("[FImageTrackerImpl] Could not get the static data for the Image Target."));
				return;
			}

			bIsTracking = true;
		}
	}

public:
	inline MLHandle Tracker()
	{
		if (!ImageTracker.IsValid())
		{
			return ML_INVALID_HANDLE;
		}
		checkf(ImageTracker.Pin().IsValid(), TEXT("[FImageTrackerImpl] ImageTracker weak pointer is invalid!"));
		return ImageTracker.Pin()->GetHandle();
	}
};

UImageTrackerComponent::UImageTrackerComponent()
	: Impl(new FImageTrackerImpl())
	, bTick(true)
{
	// Make sure this component ticks
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	bAutoActivate = true;
}

UImageTrackerComponent::~UImageTrackerComponent()
{
	delete Impl;
	Impl = nullptr;
}

void UImageTrackerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const FAppFramework& AppFramework = StaticCastSharedPtr<FMagicLeapHMD>(GEngine->XRSystem)->GetAppFrameworkConst();

	if (!AppFramework.IsInitialized())
	{
		UE_LOG(LogMagicLeap, Warning, TEXT("[UImageTrackerComponent] AppFramework not initialized."));
		return;
	}

	if (TargetImageTexture == nullptr)
	{
		UE_LOG(LogMagicLeap, Warning, TEXT("ImageTracker: No image selected to track."));
		return;
	}

	if (TargetImageTexture->GetPixelFormat() != EPixelFormat::PF_R8G8B8A8 && TargetImageTexture->GetPixelFormat() != EPixelFormat::PF_B8G8R8A8)
	{
		UE_LOG(LogMagicLeap, Error, TEXT("[UImageTrackerComponent] ImageTracker: Unsupported pixel format encountered!"));
		bTick = false;
	}

	if (!bTick) return;

	if (!Impl->bHasTarget)
	{
		if (Name.Len() == 0) Name = GetName();
		Impl->SetTargetAsync(Name, bIsStationary, LongerDimension / AppFramework.GetWorldToMetersScale(), TargetImageTexture);
	}
	else if (!Impl->OutgoingMessages.IsEmpty())
	{
		FTrackerMessage Msg;
		Impl->OutgoingMessages.Dequeue(Msg);

		if (Msg.TaskType == FTrackerMessage::TaskType::ReportStatus)
		{
			if (Msg.TrackingStatus.status == MLImageTrackerTargetStatus_NotTracked)
			{
				if (Impl->OldTrackingStatus.status != MLImageTrackerTargetStatus_NotTracked)
				{
					OnImageTargetLost.Broadcast();
				}
			}
			else
			{
				EFailReason FailReason = EFailReason::None;
				FTransform Pose = FTransform::Identity;
				if (AppFramework.GetTransform(Impl->Data.coord_frame_target, Pose, FailReason))
				{
					Pose.SetRotation(MagicLeap::ToUERotator(Pose.Rotator()).Quaternion());
					if (Msg.TrackingStatus.status == MLImageTrackerTargetStatus_Unreliable)
					{
						FVector LastTrackedLocation = GetComponentLocation();
						FRotator LastTrackedRotation = GetComponentRotation();
						if (bUseUnreliablePose)
						{
							this->SetRelativeLocationAndRotation(Pose.GetTranslation(), Pose.Rotator());
						}
						// Developer can choose whether to use this unreliable pose or not.
						OnImageTargetUnreliableTracking.Broadcast(LastTrackedLocation, LastTrackedRotation, Pose.GetTranslation(), Pose.Rotator());
					}
					else
					{
						this->SetRelativeLocationAndRotation(Pose.GetTranslation(), Pose.Rotator());
						if (Impl->OldTrackingStatus.status != MLImageTrackerTargetStatus_Tracked)
						{
							OnImageTargetFound.Broadcast();
						}
					}
				}
				else
				{
					if (FailReason == EFailReason::NaNsInTransform)
					{
						UE_LOG(LogMagicLeap, Error, TEXT("[UImageTrackerComponent] NaNs in image tracker target transform."));
					}
					Msg.TrackingStatus.status = MLImageTrackerTargetStatus_NotTracked;
					if (Impl->OldTrackingStatus.status != MLImageTrackerTargetStatus_NotTracked)
					{
						OnImageTargetLost.Broadcast();
					}
				}
			}

			Impl->OldTrackingStatus = Msg.TrackingStatus;
		}
		else
		{
			UE_LOG(LogMagicLeap, Error, TEXT("[UImageTrackerComponent] Unexpected message type recieved from worker thread."));
		}
	}
}

#if WITH_EDITOR
void UImageTrackerComponent::PreEditChange(UProperty* PropertyAboutToChange)
{
	if ((PropertyAboutToChange != nullptr) && (PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UImageTrackerComponent, TargetImageTexture)))
	{
		Impl->TextureBeforeEdit = TargetImageTexture;
	}

	Super::PreEditChange(PropertyAboutToChange);
}

void UImageTrackerComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UImageTrackerComponent, TargetImageTexture))
	{
		if (TargetImageTexture && !(TargetImageTexture->GetPixelFormat() == EPixelFormat::PF_R8G8B8A8 || TargetImageTexture->GetPixelFormat() == EPixelFormat::PF_B8G8R8A8))
		{
			TargetImageTexture = Impl->TextureBeforeEdit;
			UE_LOG(LogMagicLeap, Error, TEXT("[UImageTrackerComponent] Cannot set texture %s as it uses an invalid pixel format!  Valid formats are R8B8G8A8 or B8G8R8A8"), *TargetImageTexture->GetName());
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
