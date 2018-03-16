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

#include "MagicLeapCustomPresent.h"
#include "MagicLeapHMD.h"
#include "RenderingThread.h"
#include <ml_lifecycle.h>

#if PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_LUMIN
#include "OpenGLDrvPrivate.h"
#include "MagicLeapHelperOpenGL.h"
#endif // PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_LUMIN

#if PLATFORM_LUMIN
#include "VulkanRHIPrivate.h"
#include "MagicLeapHelperVulkan.h"
#endif // PLATFORM_LUMIN

bool FMagicLeapCustomPresent::NeedsNativePresent()
{
	return (Plugin->GetWindowMirrorMode() > 0);
}

#if PLATFORM_WINDOWS

FMagicLeapCustomPresentD3D11::FMagicLeapCustomPresentD3D11(FMagicLeapHMD* plugin)
	: FMagicLeapCustomPresent(plugin)
	, RenderTargetTexture(0)
{
}

void FMagicLeapCustomPresentD3D11::BeginRendering()
{
	check(IsInRenderingThread());
}

void FMagicLeapCustomPresentD3D11::FinishRendering()
{
	check(IsInRenderingThread());
}

void FMagicLeapCustomPresentD3D11::Reset()
{
	if (IsInGameThread())
	{
		// Wait for all resources to be released
		FlushRenderingCommands();
	}
}

void FMagicLeapCustomPresentD3D11::Shutdown()
{
	Reset();
}

void FMagicLeapCustomPresentD3D11::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());
	check(InViewportRHI);

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RT));

	// TODO Assign RenderTargetTexture here.
	InViewportRHI->SetCustomPresent(this);
}

void FMagicLeapCustomPresentD3D11::UpdateViewport_RenderThread()
{
}

void FMagicLeapCustomPresentD3D11::OnBackBufferResize()
{
}

bool FMagicLeapCustomPresentD3D11::Present(int32& SyncInterval)
{
	check(IsInRenderingThread());

	// turn off VSync for the 'normal Present'.
	SyncInterval = 0;
	bool bHostPresent = Plugin->GetWindowMirrorMode() > 0;

	FinishRendering();
	return bHostPresent;
}

#endif // PLATFORM_WINDOWS

#if PLATFORM_MAC

FMagicLeapCustomPresentMetal::FMagicLeapCustomPresentMetal(FMagicLeapHMD* plugin)
	: FMagicLeapCustomPresent(plugin)
	, RenderTargetTexture(0)
{
}

void FMagicLeapCustomPresentMetal::BeginRendering()
{
	check(IsInRenderingThread());
}

void FMagicLeapCustomPresentMetal::FinishRendering()
{
	check(IsInRenderingThread());
}

void FMagicLeapCustomPresentMetal::Reset()
{
	if (IsInGameThread())
	{
		// Wait for all resources to be released
		FlushRenderingCommands();
	}
}

void FMagicLeapCustomPresentMetal::Shutdown()
{
	Reset();
}

void FMagicLeapCustomPresentMetal::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());
	check(InViewportRHI);

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RT));

	// TODO Assign RenderTargetTexture here.
	InViewportRHI->SetCustomPresent(this);
}

void FMagicLeapCustomPresentMetal::UpdateViewport_RenderThread()
{
}

void FMagicLeapCustomPresentMetal::OnBackBufferResize()
{
}

bool FMagicLeapCustomPresentMetal::Present(int32& SyncInterval)
{
	check(IsInRenderingThread());

	// turn off VSync for the 'normal Present'.
	SyncInterval = 0;
	bool bHostPresent = Plugin->GetWindowMirrorMode() > 0;

	FinishRendering();
	return bHostPresent;
}

#endif // PLATFORM_MAC

#if PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_LUMIN

#define BEGIN_END_FRAME_BALANCE_HACK 0

#if BEGIN_END_FRAME_BALANCE_HACK
static int BalanceCounter = 0;
static MLHandle BalanceHandles[4] = { 0,0,0,0 };
static MLHandle BalancePrevFrameHandle = 0;
#endif //BEGIN_END_FRAME_BALANCE_HACK

FMagicLeapCustomPresentOpenGL::FMagicLeapCustomPresentOpenGL(FMagicLeapHMD* plugin)
	: FMagicLeapCustomPresent(plugin)
	, RenderTargetTexture(0)
	, bFramebuffersValid(false)
{
}

void FMagicLeapCustomPresentOpenGL::BeginRendering()
{
	check(IsInRenderingThread());

	FTrackingFrame* frame = Plugin->GetCurrentFrame();
	if (frame && bCustomPresentIsSet)
	{
		// TODO [Blake] : Need to see if we can use this newer matrix and override the view
		// projection matrix (since they query GetStereoProjectionMatrix on the main thread)
		MLStatus InitCameraStatus;
		MLGraphicsFrameParams camera_params;
		MLGraphicsInitFrameParams(&camera_params, &InitCameraStatus);
		camera_params.projection_type = MLGraphicsProjectionType_ReversedInfiniteZ;
		camera_params.surface_scale = frame->ScreenPercentage;
		camera_params.protected_surface = false;
		GConfig->GetBool(TEXT("/Script/LuminRuntimeSettings.LuminRuntimeSettings"), TEXT("bProtectedContent"), camera_params.protected_surface, GEngineIni);
		
		Plugin->UpdateNearClippingPlane();
		// The near clipping plane is expected in meters despite what is documented in the header.
		camera_params.near_clip = GNearClippingPlane / frame->WorldToMetersScale;
		camera_params.far_clip = frame->FarClippingPlane / frame->WorldToMetersScale;
		// Only focus distance equaling 1 engine unit seems to work on board without wearable and on desktop.
#if PLATFORM_LUMIN
		camera_params.focus_distance = frame->FocusDistance / frame->WorldToMetersScale;
#else
		camera_params.focus_distance = 1.0f;
#endif

#if BEGIN_END_FRAME_BALANCE_HACK
		if (BalanceCounter != 0)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("Begin / End frame calls out of balance!"));
			MLStatus OutStatus;
			bool bResult = MLGraphicsSignalSyncObjectGL(Plugin->GraphicsClient, BalanceHandles[0], &OutStatus);
			bResult = MLGraphicsSignalSyncObjectGL(Plugin->GraphicsClient, BalanceHandles[1], &OutStatus);
			bResult = MLGraphicsEndFrame(Plugin->GraphicsClient, BalancePrevFrameHandle, &OutStatus);
			--BalanceCounter;
		}
#endif

		MLStatus OutStatus;
		frame->bBeginFrameSucceeded = MLGraphicsBeginFrame(Plugin->GraphicsClient, &camera_params, &frame->Handle, &frame->RenderInfoArray, &OutStatus);
		if (frame->bBeginFrameSucceeded)
		{
#if BEGIN_END_FRAME_BALANCE_HACK
			++BalanceCounter;
			BalancePrevFrameHandle = frame->Handle;
			BalanceHandles[0] = frame->RenderInfoArray.virtual_cameras[0].sync_object;
			BalanceHandles[1] = frame->RenderInfoArray.virtual_cameras[1].sync_object;
#endif //BEGIN_END_FRAME_BALANCE_HACK

			/* Convert eye extents from Graphics Projection Model to Unreal Projection Model */
			// Unreal expects the projection matrix to be in centimeters and uses it for various purposes
			// such as bounding volume calculations for lights in the shadow algorithm.
			// We're overwriting the near value to match the units of unreal here instead of using the units of the SDK
			for (uint32_t eye = 0; eye < frame->RenderInfoArray.num_virtual_cameras; ++eye)
			{
				frame->RenderInfoArray.virtual_cameras[eye].projection.matrix_colmajor[10] = 0.0f; // Model change hack
				frame->RenderInfoArray.virtual_cameras[eye].projection.matrix_colmajor[11] = -1.0f; // Model change hack
				frame->RenderInfoArray.virtual_cameras[eye].projection.matrix_colmajor[14] = GNearClippingPlane; // Model change hack
			}
		}
		else
		{
			UE_LOG(LogMagicLeap, Error, TEXT("MLGraphicsBeginFrame failed with status %d"), static_cast<int32>(OutStatus));
			// TODO: See if this is only needed for ZI.
			frame->Handle = ML_INVALID_HANDLE;
			MagicLeap::ResetVirtualCameraInfoArray(frame->RenderInfoArray);
		}
	}
}

void FMagicLeapCustomPresentOpenGL::FinishRendering()
{
	check(IsInRenderingThread());

	if (Plugin->IsDeviceInitialized() && Plugin->GetCurrentFrame()->bBeginFrameSucceeded)
	{
		if (bNotifyLifecycleOfFirstPresent)
		{
			MLLifecycleErrorCode result;
			// Lifecycle tells the system's loading indicator to stop.
			// App's rendering takes over.
			bool bResult = MLLifecycleSetReadyIndication(&result);
			bNotifyLifecycleOfFirstPresent = !bResult;
			if (!bResult || result != MLLifecycleErrorCode_Success)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Error sending app ready indication to lifecycle."));
			}
		}

		// TODO [Blake] : Hack since we cannot yet specify a handle per view in the view family
		const MLGraphicsVirtualCameraInfoArray& vp_array = Plugin->GetCurrentFrame()->RenderInfoArray;
		const uint32 vp_width = static_cast<uint32>(vp_array.viewport.w);
		const uint32 vp_height = static_cast<uint32>(vp_array.viewport.h);

		if (!bFramebuffersValid)
		{
			glGenFramebuffers(2, Framebuffers);
			bFramebuffersValid = true;
		}

		GLint CurrentFB = 0;
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &CurrentFB);

		GLint FramebufferSRGB = 0;
		glGetIntegerv(GL_FRAMEBUFFER_SRGB, &FramebufferSRGB);
		if (FramebufferSRGB)
		{
			glDisable(GL_FRAMEBUFFER_SRGB);
		}

		// this texture contains both eye renders
		glBindFramebuffer(GL_FRAMEBUFFER, Framebuffers[0]);
		FOpenGL::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, RenderTargetTexture, 0);

		glBindFramebuffer(GL_FRAMEBUFFER, Framebuffers[1]);
		FOpenGL::FramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, vp_array.color_id, 0, 0);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, Framebuffers[0]);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, Framebuffers[1]);
		FOpenGL::BlitFramebuffer(0, 0, vp_width, vp_height, 0, 0, vp_width, vp_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		MLStatus OutStatus;
		bool bResult = MLGraphicsSignalSyncObjectGL(Plugin->GraphicsClient, vp_array.virtual_cameras[0].sync_object, &OutStatus);
		if (!bResult)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("MLGraphicsSignalSyncObjectGL for eye 0 failed with status %d"), static_cast<int32>(OutStatus));
		}

		FOpenGL::FramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, vp_array.color_id, 0, 1);
		FOpenGL::BlitFramebuffer(vp_width, 0, vp_width * 2, vp_height, 0, 0, vp_width, vp_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		bResult = MLGraphicsSignalSyncObjectGL(Plugin->GraphicsClient, vp_array.virtual_cameras[1].sync_object, &OutStatus);
		if (!bResult)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("MLGraphicsSignalSyncObjectGL for eye 1 failed with status %d"), static_cast<int32>(OutStatus));
		}

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, CurrentFB);
		if (FramebufferSRGB)
		{
			glEnable(GL_FRAMEBUFFER_SRGB);
		}

		for (uint32 i = 2; i < vp_array.num_virtual_cameras; ++i)
		{
			bResult = MLGraphicsSignalSyncObjectGL(Plugin->GraphicsClient, vp_array.virtual_cameras[i].sync_object, &OutStatus);
			if (!bResult)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("MLGraphicsSignalSyncObjectGL for eye %d failed with status %d"), i, static_cast<int32>(OutStatus));
			}
		}

#if BEGIN_END_FRAME_BALANCE_HACK
		--BalanceCounter;
#endif //BEGIN_END_FRAME_BALANCE_HACK

		bResult = MLGraphicsEndFrame(Plugin->GraphicsClient, Plugin->GetCurrentFrame()->Handle, &OutStatus);
		if (!bResult)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("MLGraphicsEndFrame failed with status %d"), static_cast<int32>(OutStatus));
		}
	}
	Plugin->InitializeOldFrameFromRenderFrame();
}

void FMagicLeapCustomPresentOpenGL::Reset()
{
	if (IsInGameThread())
	{
		// Wait for all resources to be released
		FlushRenderingCommands();
	}
	else if (IsInRenderingThread() && bFramebuffersValid)
	{
		glDeleteFramebuffers(2, Framebuffers);
		bFramebuffersValid = false;
	}
}

void FMagicLeapCustomPresentOpenGL::Shutdown()
{
	Reset();
}

void FMagicLeapCustomPresentOpenGL::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());
	check(InViewportRHI);

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RT));

	RenderTargetTexture = *(reinterpret_cast<uint32_t*>(RT->GetNativeResource()));
	InViewportRHI->SetCustomPresent(this);
	
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(UpdateViewport_RT,
		FMagicLeapCustomPresentOpenGL*, CustomPresent, this,
		{
			CustomPresent->UpdateViewport_RenderThread();
		}
	);
}

void FMagicLeapCustomPresentOpenGL::UpdateViewport_RenderThread()
{
	check(IsInRenderingThread());
	bCustomPresentIsSet = true;
}

void FMagicLeapCustomPresentOpenGL::OnBackBufferResize()
{
}

bool FMagicLeapCustomPresentOpenGL::Present(int& SyncInterval)
{
	check(IsInRenderingThread());

	// turn off VSync for the 'normal Present'.
	SyncInterval = 0;
	// We don't do any mirroring on Nova as we render direct to the device only.
#if PLATFORM_LUMIN
	bool bHostPresent = false;
#else
	bool bHostPresent = Plugin->GetWindowMirrorMode() > 0;
#endif

	FinishRendering();

	bCustomPresentIsSet = false;

	return bHostPresent;
}

#endif  // PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_LUMIN

#if PLATFORM_LUMIN

FMagicLeapCustomPresentVulkan::FMagicLeapCustomPresentVulkan(FMagicLeapHMD* plugin)
	: FMagicLeapCustomPresent(plugin)
	, RenderTargetTexture(0)
{
}

void FMagicLeapCustomPresentVulkan::BeginRendering()
{
	check(IsInRenderingThread());

	FTrackingFrame* frame = Plugin->GetCurrentFrame();
	if (frame && bCustomPresentIsSet)
	{
		// TODO [Blake] : Need to see if we can use this newer matrix and override the view
		// projection matrix (since they query GetStereoProjectionMatrix on the main thread)
		MLStatus InitCameraStatus;
		MLGraphicsFrameParams camera_params;
		MLGraphicsInitFrameParams(&camera_params, &InitCameraStatus);
		camera_params.projection_type = MLGraphicsProjectionType_ReversedInfiniteZ;
		camera_params.surface_scale = 1.0f;
		camera_params.protected_surface = false;
		GConfig->GetBool(TEXT("/Script/LuminRuntimeSettings.LuminRuntimeSettings"), TEXT("bProtectedContent"), camera_params.protected_surface, GEngineIni);

		// The near clipping plane is expected in meters despite what is documented in the header.
		camera_params.near_clip = GNearClippingPlane / frame->WorldToMetersScale;
		camera_params.far_clip = frame->FarClippingPlane / frame->WorldToMetersScale;

		// The focus distance is expected in meters despite what is documented in the header.
		// Only focus distance equaling 1 engine unit seems to work on board without wearable and on desktop.
#if PLATFORM_LUMIN
		camera_params.focus_distance = frame->FocusDistance / frame->WorldToMetersScale;
#else
		camera_params.focus_distance = 1.0f;
#endif

	  ExecuteOnRHIThread_DoNotWait([this, camera_params]()
	  {
		  FTrackingFrame* RHIframe = Plugin->GetCurrentFrame();
		  MLStatus OutStatus;

#if BEGIN_END_FRAME_BALANCE_HACK
			if (BalanceCounter != 0)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Begin / End frame calls out of balance!"));
				FMagicLeapHelperVulkan::SignalObjects((uint64)BalanceHandles[0], (uint64)BalanceHandles[1]);
				bool bResult = MLGraphicsEndFrame(Plugin->GraphicsClient, BalancePrevFrameHandle, &OutStatus);
				--BalanceCounter;
			}
#endif

		  RHIframe->bBeginFrameSucceeded = MLGraphicsBeginFrame(Plugin->GraphicsClient, &camera_params, &RHIframe->Handle, &RHIframe->RenderInfoArray, &OutStatus);
		  if (RHIframe->bBeginFrameSucceeded)
		  {
#if BEGIN_END_FRAME_BALANCE_HACK
				++BalanceCounter;
				BalancePrevFrameHandle = RHIframe->Handle;
				BalanceHandles[0] = RHIframe->RenderInfoArray.virtual_cameras[0].sync_object;
				BalanceHandles[1] = RHIframe->RenderInfoArray.virtual_cameras[1].sync_object;
#endif //BEGIN_END_FRAME_BALANCE_HACK
			  /* Convert eye extents from Graphics Projection Model to Unreal Projection Model */
			  // Unreal expects the projection matrix to be in centimeters and uses it for various purposes
			  // such as bounding volume calculations for lights in the shadow algorithm.
			  // We're overwriting the near value to match the units of unreal here instead of using the units of the SDK
			  for (uint32_t eye = 0; eye < RHIframe->RenderInfoArray.num_virtual_cameras; ++eye)
			  {
				  RHIframe->RenderInfoArray.virtual_cameras[eye].projection.matrix_colmajor[10] = 0.0f; // Model change hack
				  RHIframe->RenderInfoArray.virtual_cameras[eye].projection.matrix_colmajor[11] = -1.0f; // Model change hack
				  RHIframe->RenderInfoArray.virtual_cameras[eye].projection.matrix_colmajor[14] = GNearClippingPlane; // Model change hack
			  }
		  }
		  else
		  {
			  UE_LOG(LogMagicLeap, Error, TEXT("MLGraphicsBeginFrame failed with status %d"), static_cast<int32>(OutStatus));
			  // TODO: See if this is only needed for ZI.
			  RHIframe->Handle = ML_INVALID_HANDLE;
			  MagicLeap::ResetVirtualCameraInfoArray(RHIframe->RenderInfoArray);
		  }
	  });
  }
}

void FMagicLeapCustomPresentVulkan::FinishRendering()
{
	check(IsInRenderingThread() || IsInRHIThread());

	if (Plugin->IsDeviceInitialized() && Plugin->GetCurrentFrame()->bBeginFrameSucceeded)
	{
#if !PLATFORM_LUMIN
		// Set up the viewport
		if (Plugin->WindowMirrorMode == 1)
		{
			// TODO Implement this similar to OpenGL.
		}
		else if (Plugin->WindowMirrorMode == 2)
		{
			// TODO Implement this similar to OpenGL.
		}
		else if (Plugin->WindowMirrorMode == 0)
		{
			// TODO Implement this similar to OpenGL.
		}
#endif

		// TODO [Blake] : Hack since we cannot yet specify a handle per view in the view family
		const MLGraphicsVirtualCameraInfoArray& vp_array = Plugin->GetCurrentFrame()->RenderInfoArray;
		const uint32 vp_width = static_cast<uint32>(vp_array.viewport.w);
		const uint32 vp_height = static_cast<uint32>(vp_array.viewport.h);

		MLStatus OutStatus;
		bool bTestClear = false;
		if (bTestClear)
		{
			FMagicLeapHelperVulkan::TestClear((uint64)vp_array.color_id);
		}
		else
		{
			FMagicLeapHelperVulkan::BlitImage((uint64)RenderTargetTexture, 0, 0, 0, 0, vp_width, vp_height, 1, (uint64)vp_array.color_id, 0, 0, 0, 0, vp_width, vp_height, 1);
			FMagicLeapHelperVulkan::BlitImage((uint64)RenderTargetTexture, 0, vp_width, 0, 0, vp_width, vp_height, 1, (uint64)vp_array.color_id, 1, 0, 0, 0, vp_width, vp_height, 1);
		}

		FMagicLeapHelperVulkan::SignalObjects((uint64)vp_array.virtual_cameras[0].sync_object, (uint64)vp_array.virtual_cameras[1].sync_object);

#if BEGIN_END_FRAME_BALANCE_HACK
		--BalanceCounter;
#endif //BEGIN_END_FRAME_BALANCE_HACK

		bool bResult = MLGraphicsEndFrame(Plugin->GraphicsClient, Plugin->GetCurrentFrame()->Handle, &OutStatus);
		if (!bResult)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("MLGraphicsEndFrame failed with status %d"), static_cast<int32>(OutStatus));
		}
	}
  
	Plugin->InitializeOldFrameFromRenderFrame();
}

void FMagicLeapCustomPresentVulkan::Reset()
{
	if (IsInGameThread())
	{
		// Wait for all resources to be released
		FlushRenderingCommands();
	}
}

void FMagicLeapCustomPresentVulkan::Shutdown()
{
	Reset();
}

void FMagicLeapCustomPresentVulkan::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());
	check(InViewportRHI);

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RT));

	RenderTargetTexture = (VkImage)RT->GetNativeResource();
	InViewportRHI->SetCustomPresent(this);
	
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(UpdateViewport_RT,
		FMagicLeapCustomPresentVulkan*, CustomPresent, this,
		{
			CustomPresent->UpdateViewport_RenderThread();
		}
	);
}

void FMagicLeapCustomPresentVulkan::UpdateViewport_RenderThread()
{
	check(IsInRenderingThread());

	ExecuteOnRHIThread_DoNotWait([this]()
	{
		bCustomPresentIsSet = true;
	});
}

void FMagicLeapCustomPresentVulkan::OnBackBufferResize()
{
}

bool FMagicLeapCustomPresentVulkan::Present(int& SyncInterval)
{
	check(IsInRenderingThread() || IsInRHIThread());

	if (bNotifyLifecycleOfFirstPresent)
	{
		MLLifecycleErrorCode result;
		// Lifecycle tells the system's loading indicator to stop.
		// App's rendering takes over.
		bool bResult = MLLifecycleSetReadyIndication(&result);
		bNotifyLifecycleOfFirstPresent = !bResult;
		if (!bResult || result != MLLifecycleErrorCode_Success)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("Error sending app ready indication to lifecycle."));
		}
	}

	// turn off VSync for the 'normal Present'.
	SyncInterval = 0;
	// We don't do any mirroring on Nova as we render direct to the device only.
#if PLATFORM_LUMIN || PLATFORM_LUMINGL4
	bool bHostPresent = false;
#else
	bool bHostPresent = Plugin->GetWindowMirrorMode() > 0;
#endif

	FinishRendering();

	bCustomPresentIsSet = false;

	return bHostPresent;
}

#endif // PLATFORM_LUMIN
