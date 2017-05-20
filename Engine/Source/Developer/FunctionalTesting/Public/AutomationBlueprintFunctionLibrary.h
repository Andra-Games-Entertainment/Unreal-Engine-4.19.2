// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/LatentActionManager.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AutomationScreenshotOptions.h"

#include "AutomationBlueprintFunctionLibrary.generated.h"

class ACameraActor;

/**
 * 
 */
UCLASS()
class FUNCTIONALTESTING_API UAutomationBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()
	
public:
	static bool TakeAutomationScreenshotInternal(UObject* WorldContextObject, const FString& Name, FAutomationScreenshotOptions Options);

	/**
	 * Takes a screenshot of the game's viewport.  Does not capture any UI.
	 */
	UFUNCTION(BlueprintCallable, Category = "Automation", meta = (Latent, HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", LatentInfo = "LatentInfo", Name = "" ))
	static void TakeAutomationScreenshot(UObject* WorldContextObject, FLatentActionInfo LatentInfo, const FString& Name, const FAutomationScreenshotOptions& Options);

	/**
	 * Takes a screenshot of the game's viewport, from a particular camera actors POV.  Does not capture any UI.
	 */
	UFUNCTION(BlueprintCallable, Category = "Automation", meta = (Latent, HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", LatentInfo = "LatentInfo", NameOverride = "" ))
	static void TakeAutomationScreenshotAtCamera(UObject* WorldContextObject, FLatentActionInfo LatentInfo, ACameraActor* Camera, const FString& NameOverride, const FAutomationScreenshotOptions& Options);

	/**
	 * 
	 */
	UFUNCTION(BlueprintCallable, Category = "Automation", meta = ( Latent, HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", LatentInfo = "LatentInfo", NameOverride = "" ))
	static void TakeAutomationScreenshotOfUI(UObject* WorldContextObject, FLatentActionInfo LatentInfo, const FString& Name, const FAutomationScreenshotOptions& Options);

	UFUNCTION(BlueprintCallable, Category = "Automation", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static void EnableStatGroup(UObject* WorldContextObject, FName GroupName);

	UFUNCTION(BlueprintCallable, Category = "Automation", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static void DisableStatGroup(UObject* WorldContextObject, FName GroupName);

	UFUNCTION(BlueprintCallable, Category = "Automation")
	static float GetStatIncAverage(FName StatName);

	UFUNCTION(BlueprintCallable, Category = "Automation")
	static float GetStatIncMax(FName StatName);

	UFUNCTION(BlueprintCallable, Category = "Automation")
	static float GetStatExcAverage(FName StatName);

	UFUNCTION(BlueprintCallable, Category = "Automation")
	static float GetStatExcMax(FName StatName);

	UFUNCTION(BlueprintCallable, Category = "Automation")
	static float GetStatCallCount(FName StatName);

	/**
	 * Lets you know if any automated tests are running, or are about to run and the automation system is spinning up tests.
	 */
	UFUNCTION(BlueprintPure, Category="Automation")
	static bool AreAutomatedTestsRunning();

	/**
	 * 
	 */
	UFUNCTION(BlueprintPure, Category="Automation")
	static FAutomationScreenshotOptions GetDefaultScreenshotOptionsForGameplay(EComparisonTolerance Tolerance = EComparisonTolerance::Low, float Delay = 0.2);

	/**
	 * 
	 */
	UFUNCTION(BlueprintPure, Category="Automation")
	static FAutomationScreenshotOptions GetDefaultScreenshotOptionsForRendering(EComparisonTolerance Tolerance = EComparisonTolerance::Low, float Delay = 0.2);
};
