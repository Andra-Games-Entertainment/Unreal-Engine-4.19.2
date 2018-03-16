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

#include "RaycastComponent.h"
#include "MagicLeapHMD.h"
#include "MagicLeapMath.h"
#include "AppFramework.h"
#include "Engine/Engine.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

#include <ml_raycast.h>

class FRaycastTrackerImpl
{
public:
	FRaycastTrackerImpl()
		: Tracker(ML_INVALID_HANDLE)
	{};

public:
	MLHandle Tracker;

public:
	bool Create()
	{
		if (!MLHandleIsValid(Tracker))
		{
			Tracker = MLRaycastCreate();
			if (!MLHandleIsValid(Tracker))
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Error creating raycast tracker."));
				return false;
			}
		}
		return true;
	}

	void Destroy()
	{
		if (MLHandleIsValid(Tracker))
		{
			bool bResult = MLRaycastDestroy(Tracker);
			if (!bResult)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Error destroying raycast tracker."));
			}
			Tracker = ML_INVALID_HANDLE;
		}
	}
};

ERaycastResultState MLToUnrealRaycastResultState(MLRaycastResultState state)
{
	switch (state)
	{
	case MLRaycastResultState_RequestFailed:
		return ERaycastResultState::RequestFailed;
	case MLRaycastResultState_HitObserved:
		return ERaycastResultState::HitObserved;
	case MLRaycastResultState_HitUnobserved:
		return ERaycastResultState::HitUnobserved;
	case MLRaycastResultState_NoCollision:
		return ERaycastResultState::NoCollission;
	}
	return ERaycastResultState::RequestFailed;
}

URaycastComponent::URaycastComponent()
	: Impl(new FRaycastTrackerImpl())
{
	// Make sure this component ticks
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bAutoActivate = true;

#if WITH_EDITOR
	if (GIsEditor)
	{
		FEditorDelegates::PrePIEEnded.AddUObject(this, &URaycastComponent::PrePIEEnded);
	}
#endif
}

URaycastComponent::~URaycastComponent()
{
	delete Impl;
}

void URaycastComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!MLHandleIsValid(Impl->Tracker))
	{
		return;
	}

	for (auto& pair : PendingRequests)
	{
		MLRaycastResult result;
		if (MLRaycastGetResult(Impl->Tracker, pair.Key, &result))
		{
			const FAppFramework& AppFramework = static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
			float WorldToMetersScale = AppFramework.IsInitialized() ? AppFramework.GetWorldToMetersScale() : 100.0f;

			// TODO: Should we apply this transform here or expect the user to use the result as a child of the XRPawn like the other features?
			// This being for raycast, we should probably apply the transform since the result might be used for other than just placing objects.
			FTransform Pose = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(this);

			FRaycastHitResult hitResult;
			hitResult.HitState = MLToUnrealRaycastResultState(result.state);
			hitResult.HitPoint = Pose.TransformPosition(MagicLeap::ToFVector(result.hitpoint, WorldToMetersScale));
			hitResult.Normal = Pose.TransformVectorNoScale(MagicLeap::ToFVector(result.normal, 1.0f));
			hitResult.Confidence = result.confidence;
			hitResult.UserData = pair.Value.UserData;

			if (hitResult.HitPoint.ContainsNaN() || hitResult.Normal.ContainsNaN())
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Raycast result contains NaNs."));
				hitResult.HitState = ERaycastResultState::RequestFailed;
			}

			pair.Value.ResultDelegate.ExecuteIfBound(hitResult);
			CompletedRequests.Add(pair.Key);
		}
	}

	// TODO: Implement better strategy to optimize memory allocation.
	if (CompletedRequests.Num() > 0)
	{
		for (MLHandle handle : CompletedRequests)
		{
			PendingRequests.Remove(handle);
		}
		CompletedRequests.Empty();
	}
}

bool URaycastComponent::RequestRaycast(const FRaycastQueryParams& RequestParams, const FRaycastResultDelegate& ResultDelegate)
{
	if (!Impl->Create())
	{
		return false;
	}

	const FAppFramework& AppFramework = static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
	float WorldToMetersScale = AppFramework.IsInitialized() ? AppFramework.GetWorldToMetersScale() : 100.0f;

	FTransform PoseInverse = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(this).Inverse();

	MLRaycastQuery query;
	query.position = MagicLeap::ToMLVector(PoseInverse.TransformPosition(RequestParams.Position), WorldToMetersScale);
	query.direction = MagicLeap::ToMLVectorNoScale(PoseInverse.TransformVectorNoScale(RequestParams.Direction));
	query.up_vector = MagicLeap::ToMLVectorNoScale(PoseInverse.TransformVectorNoScale(RequestParams.UpVector));
	query.width = static_cast<uint32>(RequestParams.Width);
	query.height = static_cast<uint32>(RequestParams.Height);
	query.collide_with_unobserved = RequestParams.CollideWithUnobserved;
	query.horizontal_fov_degrees = RequestParams.HorizontalFovDegrees;

	MLHandle handle = MLRaycastRequest(Impl->Tracker, &query);
	if (!MLHandleIsValid(handle))
	{
		UE_LOG(LogMagicLeap, Error, TEXT("Could not request raycast."));
		return false;
	}

	FRaycastRequestMetaData& requestMetaData = PendingRequests.Add(handle);
	requestMetaData.UserData = RequestParams.UserData;
	requestMetaData.ResultDelegate = ResultDelegate;

	return true;
}

void URaycastComponent::FinishDestroy()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		FEditorDelegates::PrePIEEnded.RemoveAll(this);
	}
#endif
	Impl->Destroy();
	Super::FinishDestroy();
}

#if WITH_EDITOR
void URaycastComponent::PrePIEEnded(bool bWasSimulatingInEditor)
{
	Impl->Destroy();
}
#endif
