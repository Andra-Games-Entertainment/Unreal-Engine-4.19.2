// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "VIBaseTransformGizmo.h"
#include "VREditorTransformGizmo.generated.h"

/**
 * A transform gizmo that allows you to interact with selected objects by moving, scaling and rotating.
 */
UCLASS()
class ATransformGizmo : public ABaseTransformGizmo
{
	GENERATED_BODY()

public:

	/** Default constructor that sets up CDO properties */
	ATransformGizmo();

	/** Called by the world interaction system after we've been spawned into the world, to allow
	    us to create components and set everything up nicely for the selected objects that we'll be
		used to manipulate */
	virtual void UpdateGizmo(const EGizmoHandleTypes InGizmoType, const ECoordSystem InGizmoCoordinateSpace, const FTransform& InLocalToWorld, const FBox& InLocalBounds, 
		const FVector& InViewLocation, const float InScaleMultiplier, bool bInAllHandlesVisible, const bool bInAllowRotationAndScaleHandles, class UActorComponent* DraggingHandle, 
		const TArray<UActorComponent*>& InHoveringOverHandles, const float InGizmoHoverScale, const float InGizmoHoverAnimationDuration) override;

private:

	/** Measurements, one for each axis */
	UPROPERTY()
	FTransformGizmoMeasurement Measurements[ 3 ];

	/** Bounding box translation handles */
	UPROPERTY()
	class UVREditorTranslationGizmoHandleGroup* TranslationGizmoHandleGroup;

	/** Bounding box plane translation handles */
	UPROPERTY()
	class UVREditorPlaneTranslationGizmoHandleGroup* PlaneTranslationGizmoHandleGroup;

	/** Bounding box rotation handles */
	UPROPERTY()
	class UVREditorRotationGizmoHandleGroup* RotationGizmoHandleGroup;

	/** Bounding box stretch handles */
	UPROPERTY()
	class UStretchGizmoHandleGroup* StretchGizmoHandleGroup;

	/** Bounding box uniform scale handle */
	UPROPERTY()
	class UUniformScaleGizmoHandleGroup* UniformScaleGizmoHandleGroup;
};
