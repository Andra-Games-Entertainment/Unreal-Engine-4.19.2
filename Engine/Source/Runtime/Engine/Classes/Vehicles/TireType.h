// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/DataAsset.h"
#include "TireType.generated.h"

/** DEPRECATED - Only used to allow conversion to new TireConfig in PhysXVehicles plugin */
UCLASS()
class ENGINE_API UTireType : public UDataAsset
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleAnywhere, Category = Deprecated)
	float FrictionScale;
};
