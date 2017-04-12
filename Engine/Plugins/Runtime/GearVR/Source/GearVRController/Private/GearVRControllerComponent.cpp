// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../Public/GearVRControllerComponent.h"
#include "../Public/GearVRControllerFunctionLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "MotionControllerComponent.h"

UGearVRControllerComponent::UGearVRControllerComponent()
: ControllerMesh(nullptr)
, MotionControllerComponent(nullptr)
, ControllerMeshComponent(nullptr)
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;


	ControllerMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, TEXT("/GearVR/Meshes/GearVRController")));
}

UMotionControllerComponent* UGearVRControllerComponent::GetMotionController() const
{
	return MotionControllerComponent;
}

UStaticMeshComponent* UGearVRControllerComponent::GetControllerMesh() const
{
	return ControllerMeshComponent;
}

void UGearVRControllerComponent::OnRegister()
{
	Super::OnRegister();

	check(ControllerMesh != nullptr);

	MotionControllerComponent = NewObject<UMotionControllerComponent>(this, TEXT("MotionController"));
	MotionControllerComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MotionControllerComponent->SetupAttachment(this);
	MotionControllerComponent->RegisterComponent();

	ControllerMeshComponent = NewObject<UStaticMeshComponent>(this, TEXT("ControllerMesh"));
	ControllerMeshComponent->SetStaticMesh(ControllerMesh);
	ControllerMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ControllerMeshComponent->SetupAttachment(MotionControllerComponent);
	ControllerMeshComponent->RegisterComponent();
	ControllerMeshComponent->SetRelativeRotation(FQuat::MakeFromEuler(FVector(0.0f, 0.0f, 90.0f)));
}

void UGearVRControllerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bool isActive = UGearVRControllerFunctionLibrary::IsControllerActive();

	if (MotionControllerComponent != nullptr)
	{
		MotionControllerComponent->SetActive(isActive);
		MotionControllerComponent->SetVisibility(isActive);
	}

	if (ControllerMeshComponent != nullptr)
	{
		ControllerMeshComponent->SetActive(isActive);
		ControllerMeshComponent->SetVisibility(isActive);
	}
}

void UGearVRControllerComponent::BeginPlay()
{
	Super::BeginPlay();
}