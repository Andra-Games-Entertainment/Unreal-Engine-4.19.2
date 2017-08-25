// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "FunctionalTest.h"
#include "AutomationScreenshotOptions.h"

#include "ScreenshotFunctionalTest.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class FUNCTIONALTESTING_API AScreenshotFunctionalTest : public AFunctionalTest
{
	GENERATED_BODY()

public:
	AScreenshotFunctionalTest(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	virtual bool CanEditChange(const UProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// Tests relying on temporal effects can force a camera cut to flush stale data
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", SimpleDisplay)
	bool bCameraCutOnScreenshotPrep;

protected:
	virtual void PrepareTest() override;
	virtual bool IsReady_Implementation() override;
	virtual void StartTest() override;

	void OnScreenshotTakenAndCompared();
	
protected:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", SimpleDisplay)
	class UCameraComponent* ScreenshotCamera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Screenshot", SimpleDisplay)
	FAutomationScreenshotOptions ScreenshotOptions;
};
