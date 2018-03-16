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

#include "PlanesComponent.h"
#include "Components/BoxComponent.h"
#include "MagicLeapHMD.h"
#include "AppFramework.h"
#include "MagicLeapMath.h"
#include "Engine/Engine.h"
#include "Kismet/KismetMathLibrary.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

#include "ml_planes.h"

class FPlanesTrackerImpl
{
public:
	FPlanesTrackerImpl()
		: Tracker(ML_INVALID_HANDLE)
	{};

public:
	MLHandle Tracker;

public:
	bool Create()
	{
		if (!MLHandleIsValid(Tracker))
		{
			UE_LOG(LogMagicLeap, Display, TEXT("Creating Planes Tracker"));
			Tracker = MLPlanesCreate();

			if (!MLHandleIsValid(Tracker))
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Could not create planes tracker."));
				return false;
			}
		}
		return true;
	}

	void Destroy()
	{
		if (MLHandleIsValid(Tracker))
		{
			bool bResult = MLPlanesDestroy(Tracker);
			if (!bResult)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Error destroying planes tracker."));
			}
			Tracker = ML_INVALID_HANDLE;
		}
	}
};

MLPlanesQueryFlags UnrealToMLPlanesQueryFlagMap(EPlaneQueryFlags QueryFlag)
{
	switch (QueryFlag)
	{
	case EPlaneQueryFlags::Vertical:
		return MLPlanesQueryFlag_Vertical;
	case EPlaneQueryFlags::Horizontal:
		return MLPlanesQueryFlag_Horizontal;
	case EPlaneQueryFlags::Arbitrary:
		return MLPlanesQueryFlag_Arbitrary;
	case EPlaneQueryFlags::OrientToGravity:
		return MLPlanesQueryFlag_OrientToGravity;
	case EPlaneQueryFlags::PreferInner:
		return MLPlanesQueryFlag_Inner;
	case EPlaneQueryFlags::IgnoreHoles:
		return MLPlanesQueryFlag_IgnoreHoles;
	case EPlaneQueryFlags::Ceiling:
		return MLPlanesQueryFlag_Semantic_Ceiling;
	case EPlaneQueryFlags::Floor:
		return MLPlanesQueryFlag_Semantic_Floor;
	case EPlaneQueryFlags::Wall:
		return MLPlanesQueryFlag_Semantic_Wall;
	}
	return static_cast<MLPlanesQueryFlags>(0);
}

EPlaneQueryFlags MLToUnrealPlanesQueryFlagMap(MLPlanesQueryFlags QueryFlag)
{
	switch (QueryFlag)
	{
	case MLPlanesQueryFlag_Vertical:
		return EPlaneQueryFlags::Vertical;
	case MLPlanesQueryFlag_Horizontal:
		return EPlaneQueryFlags::Horizontal;
	case MLPlanesQueryFlag_Arbitrary:
		return EPlaneQueryFlags::Arbitrary;
	case MLPlanesQueryFlag_OrientToGravity:
		return EPlaneQueryFlags::OrientToGravity;
	case MLPlanesQueryFlag_Inner:
		return EPlaneQueryFlags::PreferInner;
	case MLPlanesQueryFlag_IgnoreHoles:
		return EPlaneQueryFlags::IgnoreHoles;
	case MLPlanesQueryFlag_Semantic_Ceiling:
		return EPlaneQueryFlags::Ceiling;
	case MLPlanesQueryFlag_Semantic_Floor:
		return EPlaneQueryFlags::Floor;
	case MLPlanesQueryFlag_Semantic_Wall:
		return EPlaneQueryFlags::Wall;
	}
	return static_cast<EPlaneQueryFlags>(0);
}

MLPlanesQueryFlags UnrealToMLPlanesQueryFlags(const TArray<EPlaneQueryFlags>& QueryFlags)
{
	MLPlanesQueryFlags mlflags = static_cast<MLPlanesQueryFlags>(0);

	for (EPlaneQueryFlags flag : QueryFlags)
	{
		mlflags = static_cast<MLPlanesQueryFlags>(mlflags | UnrealToMLPlanesQueryFlagMap(flag));
	}

	return mlflags;
}

void MLToUnrealPlanesQueryFlags(uint32 QueryFlags, TArray<EPlaneQueryFlags>& OutPlaneFlags)
{
	static TArray<MLPlanesQueryFlags> AllMLFlags({
	  MLPlanesQueryFlag_Vertical,
	  MLPlanesQueryFlag_Horizontal,
	  MLPlanesQueryFlag_Arbitrary,
	  MLPlanesQueryFlag_OrientToGravity,
	  MLPlanesQueryFlag_Inner,
	  MLPlanesQueryFlag_IgnoreHoles,
	  MLPlanesQueryFlag_Semantic_Ceiling,
	  MLPlanesQueryFlag_Semantic_Floor,
	  MLPlanesQueryFlag_Semantic_Wall
	});

	OutPlaneFlags.Empty();

	for (MLPlanesQueryFlags flag : AllMLFlags)
	{
		if ((QueryFlags & static_cast<MLPlanesQueryFlags>(flag)) != 0)
		{
			OutPlaneFlags.Add(MLToUnrealPlanesQueryFlagMap(flag));
		}
	}
}

UPlanesComponent::UPlanesComponent()
	: QueryFlags({ EPlaneQueryFlags::Vertical, EPlaneQueryFlags::Horizontal, EPlaneQueryFlags::Arbitrary })
	, MaxResults(1)
	, MinHolePerimeter(50.0f)
	, MinPlaneArea(400.0f)
	, IgnoreBoundingVolume(false)
	, Impl(new FPlanesTrackerImpl())
{
	// Make sure this component ticks
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bAutoActivate = true;

	SearchVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SearchVolume"));
	SearchVolume->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	SearchVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SearchVolume->SetCanEverAffectNavigation(false);
	SearchVolume->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;
	SearchVolume->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	SearchVolume->bGenerateOverlapEvents = false;

#if WITH_EDITOR
	if (GIsEditor)
	{
		FEditorDelegates::PrePIEEnded.AddUObject(this, &UPlanesComponent::PrePIEEnded);
	}
#endif
}

UPlanesComponent::~UPlanesComponent()
{
	delete Impl;
	Impl = nullptr;
}

void UPlanesComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Impl->Create())
	{
		return;
	}

	FTransform PoseTransform = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(this);

	for (auto& pair : PendingRequests)
	{
		uint32 outNumResults = 0;
		TArray<MLPlane> resultMLPlanes;
		resultMLPlanes.AddDefaulted(pair.Value.MaxResults);

		MLPlanesQueryResult result = MLPlanesQueryGetResults(Impl->Tracker, pair.Key, resultMLPlanes.GetData(), &outNumResults);
		switch (result)
		{
		case MLPlanesQueryResult_Failure:
		{
			pair.Value.ResultDelegate.ExecuteIfBound(false, TArray<FPlaneResult>(), pair.Value.UserData);
			CompletedRequests.Add(pair.Key);
			break;
		}
		case MLPlanesQueryResult_Success:
		{
			const FAppFramework& AppFramework = static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
			float WorldToMetersScale = AppFramework.GetWorldToMetersScale();

			TArray<FPlaneResult> Planes;
			Planes.Reserve(outNumResults);

			for (uint32 i = 0; i < outNumResults; ++i)
			{
				FPlaneResult resultPlane;
				// Perception uses all coordinates in RUB so for them X axis is right and corresponds to the width of the plane.
				// Unreal uses FRU, so the Y-axis is towards the right which makes the Y component of the vector the width.
				resultPlane.PlaneDimensions = FVector2D(resultMLPlanes[i].height * WorldToMetersScale, resultMLPlanes[i].width * WorldToMetersScale);

				FTransform planeTransform = FTransform(MagicLeap::ToFQuat(resultMLPlanes[i].rotation), MagicLeap::ToFVector(resultMLPlanes[i].position, WorldToMetersScale), FVector(1.0f, 1.0f, 1.0f));
				if (planeTransform.ContainsNaN())
				{
					UE_LOG(LogMagicLeap, Error, TEXT("Plane result %d transform contains NaN."), i);
					continue;
				}
				if (!planeTransform.GetRotation().IsNormalized())
				{
					FQuat rotation = planeTransform.GetRotation();
					rotation.Normalize();
					planeTransform.SetRotation(rotation);
				}
				planeTransform.AddToTranslation(PoseTransform.GetLocation());
				planeTransform.ConcatenateRotation(PoseTransform.Rotator().Quaternion());
				resultPlane.PlanePosition = planeTransform.GetLocation();
				resultPlane.PlaneOrientation = MagicLeap::ToUERotator(planeTransform.Rotator());
				// The plane orientation has the forward axis (X) pointing in the direction of the plane's normal.
				// We are rotating it by 90 degrees clock-wise about the right axis (Y) to get the up vector (Z) to point in the direction of the plane's normal.
				// Since we are rotating the axis, the rotation is in the opposite direction of the object i.e. -90 degrees.
				resultPlane.ContentOrientation = UKismetMathLibrary::Conv_VectorToRotator(UKismetMathLibrary::RotateAngleAxis(UKismetMathLibrary::Conv_RotatorToVector(resultPlane.PlaneOrientation), -90, UKismetMathLibrary::GetRightVector(resultPlane.PlaneOrientation)));
				MLToUnrealPlanesQueryFlags(resultMLPlanes[i].flags, resultPlane.PlaneFlags);

				Planes.Add(resultPlane);
			}

			pair.Value.ResultDelegate.ExecuteIfBound(true, Planes, pair.Value.UserData);
			CompletedRequests.Add(pair.Key);
			break;
		}
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

bool UPlanesComponent::RequestPlanes(int32 UserData, const FPlaneResultDelegate& ResultDelegate)
{
	const FAppFramework& AppFramework = static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
	float WorldToMetersScale = AppFramework.GetWorldToMetersScale();
	check(WorldToMetersScale != 0);

	FTransform PoseInverse = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(this).Inverse();
	FPlane plane;

	MLPlanesQuery query;
	query.max_results = static_cast<uint32>(MaxResults);
	query.flags = UnrealToMLPlanesQueryFlags(QueryFlags);
	query.min_hole_length = MinHolePerimeter / WorldToMetersScale;
	query.min_plane_area = MinPlaneArea / (WorldToMetersScale * WorldToMetersScale);
	PoseInverse.ConcatenateRotation(SearchVolume->GetComponentQuat());
	query.bounds_center = MagicLeap::ToMLVector(PoseInverse.TransformPosition(SearchVolume->GetComponentLocation()), WorldToMetersScale);
	query.bounds_rotation = MagicLeap::ToMLQuat(PoseInverse.GetRotation());
	query.bounds_extents = MagicLeap::ToMLVector(SearchVolume->GetScaledBoxExtent(), WorldToMetersScale);
	// MagicLeap::ToMLVector() causes the Z component to be negated.
	// The bounds were thus invalid and resulted in everything being tracked. 
	// This provides the content devs with an option to ignore the bounding volume at will.
	// TODO: Can this be improved?
	if (!IgnoreBoundingVolume)
	{
		query.bounds_extents.x = FMath::Abs<float>(query.bounds_extents.x);
		query.bounds_extents.y = FMath::Abs<float>(query.bounds_extents.y);
		query.bounds_extents.z = FMath::Abs<float>(query.bounds_extents.z);
	}

	MLHandle handle = MLPlanesQueryBegin(Impl->Tracker, &query);
	if (!MLHandleIsValid(handle))
	{
		UE_LOG(LogMagicLeap, Error, TEXT("Could not request planes."));
		return false;
	}

	FPlanesRequestMetaData& requestMetaData = PendingRequests.Add(handle);
	requestMetaData.MaxResults = query.max_results;
	requestMetaData.UserData = UserData;
	requestMetaData.ResultDelegate = ResultDelegate;

	return true;
}

void UPlanesComponent::FinishDestroy()
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
void UPlanesComponent::PrePIEEnded(bool bWasSimulatingInEditor)
{
	Impl->Destroy();
}
#endif
