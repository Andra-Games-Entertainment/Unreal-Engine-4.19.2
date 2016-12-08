// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GearVRFunctionLibrary.generated.h"

UCLASS()
class UGearVRFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:

	/** Set Gear VR CPU and GPU Levels */
	UFUNCTION(BlueprintCallable, Category = "GearVR")
	static void SetCPUAndGPULevels(int CPULevel, int GPULevel);

	/** Set Gear VR CPU and GPU Levels */
	UFUNCTION(BlueprintCallable, Category = "GearVR")
	static bool IsPowerLevelStateMinimum();

	/** Set Gear VR CPU and GPU Levels */
	UFUNCTION(BlueprintCallable, Category = "GearVR")
	static bool IsPowerLevelStateThrottled();

	/** Set Gear VR CPU and GPU Levels */
	UFUNCTION(BlueprintCallable, Category = "GearVR")
	static bool AreHeadPhonesPluggedIn();

	/** Set Gear VR CPU and GPU Levels */
	UFUNCTION(BlueprintCallable, Category = "GearVR")
	static float GetTemperatureInCelsius();

	/** Set Gear VR CPU and GPU Levels */
	UFUNCTION(BlueprintCallable, Category = "GearVR")
	static float GetBatteryLevel();
};
