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

#include "AppFramework.h"
#include "MagicLeapHMD.h"
#include "GameFramework/WorldSettings.h"
#include "RenderingThread.h"
#include "Engine/Engine.h"
#include "Misc/CoreDelegates.h"

#include <ml_snapshot.h>

TArray<MagicLeap::IAppEventHandler*> FAppFramework::EventHandlers;

FAppFramework::FAppFramework()
{}

FAppFramework::~FAppFramework()
{
	Shutdown();
}

void FAppFramework::Startup()
{
	base_dirty_ = false;

	base_coordinate_frame_.data[0] = 0;
	base_coordinate_frame_.data[1] = 0;

	base_position_ = FVector::ZeroVector;
	base_orientation_ = FQuat::Identity;

	for (auto EventHandler : EventHandlers)
	{
		EventHandler->OnAppStartup();
	}

	// Register application lifecycle delegates
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FAppFramework::ApplicationPauseDelegate);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FAppFramework::ApplicationResumeDelegate);

	bInitialized = true;

	saved_max_fps_ = 0.0f;
}

void FAppFramework::Shutdown()
{
	bInitialized = false;
}

void FAppFramework::BeginUpdate()
{
	if (bInitialized)
	{
		if (base_dirty_)
		{
			MLTransform Transform = MagicLeap::kIdentityTransform;
			Transform.position = MagicLeap::ToMLVector(-base_position_, GetWorldToMetersScale());
			Transform.rotation = MagicLeap::ToMLQuat(base_orientation_.Inverse());

			base_coordinate_frame_.data[0] = 0;
			base_coordinate_frame_.data[1] = 0;
			base_position_ = FVector::ZeroVector;
			base_orientation_ = FQuat::Identity;
			base_dirty_ = false;
		}
	}
}

void FAppFramework::ApplicationPauseDelegate()
{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("+++++++ AppFramework APP PAUSE ++++++"));

	FlushRenderingCommands();

	FMagicLeapHMD * hmd = GEngine ? static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice()) : nullptr;

	if (hmd && hmd->GetActiveCustomPresent())
	{
		hmd->GetActiveCustomPresent()->Reset();
	}

	saved_max_fps_ = GEngine->GetMaxFPS();
	GEngine->SetMaxFPS(0.0f);

	for (auto EventHandler : EventHandlers)
	{
		EventHandler->OnAppPause();
	}
}

void FAppFramework::ApplicationResumeDelegate()
{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("+++++++ MLContext APP RESUME ++++++"));
	GEngine->SetMaxFPS(saved_max_fps_);

	for (auto EventHandler : EventHandlers)
	{
		EventHandler->OnAppResume();
	}
}

void FAppFramework::OnApplicationShutdown()
{
	for (auto EventHandler : EventHandlers)
	{
		EventHandler->OnAppShutDown();
	}
}

void FAppFramework::SetBaseCoordinateFrame(MLCoordinateFrameUID InBaseCoordinateFrame)
{
	base_coordinate_frame_ = InBaseCoordinateFrame;
	base_dirty_ = true;
}

void FAppFramework::SetBasePosition(const FVector& InBasePosition)
{
	base_position_ = InBasePosition;
	base_dirty_ = true;
}

void FAppFramework::SetBaseOrientation(const FQuat& InBaseOrientation)
{
	base_orientation_ = InBaseOrientation;
	base_dirty_ = true;
}

void FAppFramework::SetBaseRotation(const FRotator& InBaseRotation)
{
	base_orientation_ = InBaseRotation.Quaternion();
	base_dirty_ = true;
}

FTrackingFrame* FAppFramework::GetCurrentFrame() const
{
	FMagicLeapHMD* hmd = GEngine ? static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice()) : nullptr;
	return hmd ? hmd->GetCurrentFrame() : nullptr;
}

FTrackingFrame* FAppFramework::GetOldFrame() const
{
	FMagicLeapHMD* hmd = GEngine ? static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice()) : nullptr;
	return hmd ? hmd->GetOldFrame() : nullptr;
}

FVector2D FAppFramework::GetFieldOfView() const
{
	// TODO Pass correct values when graphics provides them through API.
	return FVector2D(80.0f, 60.0f);
}

bool FAppFramework::GetDeviceResolution(FVector2D& OutResolution) const
{
	const FTrackingFrame *frame = GetOldFrame();

	if (frame && frame->RenderInfoArray.num_virtual_cameras > 0)
	{
		const float width = frame->RenderInfoArray.viewport.w * 2.0f;
		const float height = frame->RenderInfoArray.viewport.h;
		OutResolution = FVector2D(width, height);
		return true;
	}

	OutResolution = FVector2D(1280.0f * 2.0f, 960.0f);
	return false;
}

uint32 FAppFramework::GetViewportCount() const
{
	const FTrackingFrame *frame = GetOldFrame();
	return frame ? frame->RenderInfoArray.num_virtual_cameras : 2;
}

float FAppFramework::GetWorldToMetersScale() const
{
	const FTrackingFrame *frame = GetCurrentFrame();

	// If the frame is not ready, return the default system scale.
	if (!frame)
	{
		return GWorld ? GWorld->GetWorldSettings()->WorldToMeters : 100.0f;
	}

	return frame->WorldToMetersScale;
}

FTransform FAppFramework::GetCurrentFrameUpdatePose() const
{
	FTrackingFrame* frame = GetCurrentFrame();
	return frame ? frame->RawPose : FTransform::Identity;
}

bool FAppFramework::GetTransform(const MLCoordinateFrameUID& Id, FTransform& OutTransform, EFailReason& OutReason) const
{
	FTrackingFrame* frame = GetCurrentFrame();
	if (frame == nullptr)
	{
		OutReason = EFailReason::InvalidTrackingFrame;
		return false;
	}

	MLTransform transform = MagicLeap::kIdentityTransform;
	if (MLSnapshotGetTransform(frame->Snapshot, &Id, &transform))
	{
		OutTransform = MagicLeap::ToFTransform(transform, GetWorldToMetersScale());
		if (OutTransform.ContainsNaN())
		{
			UE_LOG(LogMagicLeap, Error, TEXT("MLSnapshotGetTransform() returned an invalid transform with NaNs."));
			OutReason = EFailReason::NaNsInTransform;
			return false;
		}
		// Unreal crashes if the incoming quaternion is not normalized.
		if (!OutTransform.GetRotation().IsNormalized())
		{
			FQuat rotation = OutTransform.GetRotation();
			rotation.Normalize();
			OutTransform.SetRotation(rotation);
		}
		OutReason = EFailReason::None;
		return true;
	}
	OutReason = EFailReason::CallFailed;
	return false;
}

void FAppFramework::AddEventHandler(MagicLeap::IAppEventHandler* EventHandler)
{
	EventHandlers.Add(EventHandler);
}

void FAppFramework::RemoveEventHandler(MagicLeap::IAppEventHandler* EventHandler)
{
	EventHandlers.Remove(EventHandler);
}
