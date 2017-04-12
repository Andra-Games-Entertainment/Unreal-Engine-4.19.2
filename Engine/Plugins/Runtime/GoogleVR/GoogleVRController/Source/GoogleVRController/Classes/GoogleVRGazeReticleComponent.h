/* Copyright 2016 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "Components/ActorComponent.h"
#include "GoogleVRPointer.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInterface.h"
#include "Components/StaticMeshComponent.h"
#include "GoogleVRGazeReticleComponent.generated.h"

class UGoogleVRPointerInputComponent;
class UCameraComponent;

DEFINE_LOG_CATEGORY_STATIC(LogGoogleVRGazeReticle, Log, All);

/**
 * GoogleVRGazeReticleComponent is a customizable reticle used to interact with
 * actors and widgets by looking at them. Intended for use with Google Cardboard appliations.
 *
 * This class integrates with GoogleVRPointerInputComponent so that the reticle can easily be used to interact with
 * Actors and widgets.
 */
UCLASS(ClassGroup=(GoogleVRController), meta=(BlueprintSpawnableComponent))
class GOOGLEVRCONTROLLER_API UGoogleVRGazeReticleComponent : public USceneComponent, public IGoogleVRPointer
{
	GENERATED_BODY()

public:

	UGoogleVRGazeReticleComponent();

	/** Mesh used for the reticle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reticle")
	UStaticMesh* Mesh;

	/** Material used for the reticle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reticle")
	UMaterialInterface* Material;

	/** Minimum distance of the reticle (in meters). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reticle")
	float ReticleDistanceMin;

	/** Maximum distance of the reticle (in meters). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reticle")
	float ReticleDistanceMax;

	/** A float to adjust the size of this reticle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reticle")
	float ReticleSize;

	/** Minimum inner angle of the reticle (in degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reticle")
	float ReticleInnerAngleMin;

	/** Minimum outer angle of the reticle (in degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reticle")
	float ReticleOuterAngleMin;

	/** Angle at which to expand the reticle when intersecting with an object (in degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reticle")
	float ReticleGrowAngle;

	/** Growth speed multiplier for the reticle when it is expanding & contracting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reticle")
	float ReticleGrowSpeed;

	/** If true, then a GoogleVRInputComponent will automatically be created if one doesn't already exist. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool RequireInputComponent;

	/** IGoogleVRPointer Implementation */
	virtual void OnPointerEnter(const FHitResult& HitResult, bool IsHitInteractive) override;
	virtual void OnPointerHover(const FHitResult& HitResult, bool IsHitInteractive) override;
	virtual void OnPointerExit(const FHitResult& HitResult) override;
	virtual FVector GetOrigin() const override;
	virtual FVector GetDirection() const override;
	virtual void GetRadius(float& OutEnterRadius, float& OutExitRadius) const override;
	virtual float GetMaxPointerDistance() const override;
	virtual bool IsPointerActive() const override;

	/** ActorComponent Overrides */
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void Activate(bool bReset=false) override;
	virtual void Deactivate() override;
	virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;

private:

	float GetWorldToMetersScale() const;

	void SetReticleExpanded(bool NewIsReticleExpanded);
	float CalculateReticleDiameter(float ReticleAngleDegrees);
	void UpdateReticleDiameter(float DeltaTime);
	void SetReticleEnabled(bool NewEnabled);

	UStaticMeshComponent* ReticleMeshComponent;
	UCameraComponent* CameraComponent;

	float ReticleInnerDiameter;
	float ReticleOuterDiameter;
	float TargetReticleInnerDiameter;
	float TargetReticleOuterDiameter;
	float CurrentReticleDistance;
	bool IsReticleExpanded;
};
