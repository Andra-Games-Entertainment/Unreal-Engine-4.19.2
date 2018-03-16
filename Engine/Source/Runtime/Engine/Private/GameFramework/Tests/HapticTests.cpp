// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
//

#include "Package.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "IInputInterface.h"
#include "IMotionController.h"
#include "SlateApplication.h"
#include "Haptics/HapticFeedbackEffect_Base.h"

DEFINE_LOG_CATEGORY_STATIC(LogHapticTest, Display, All);

#if WITH_DEV_AUTOMATION_TESTS

/**
* Play Low Level Buffer Driven Haptic Effect
*/
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FPlayBufferHapticLatentCommand, EControllerHand, Hand, TSharedPtr<struct FActiveHapticFeedbackEffect>, ActiveHapticEffect);
bool FPlayBufferHapticLatentCommand::Update()
{
	const int32 ControllerId = 0;

	IInputInterface* InputInterface = FSlateApplication::Get().GetInputInterface();
	if (InputInterface)
	{
		FHapticFeedbackValues HapticValues;
		if (ActiveHapticEffect.IsValid())
		{
			const bool bPlaying = ActiveHapticEffect->Update(FSlateApplication::Get().GetDeltaTime(), HapticValues);
			if (!bPlaying)
			{
				ActiveHapticEffect.Reset();
			}
		}
		InputInterface->SetHapticFeedbackValues(ControllerId, (int32)Hand, HapticValues);
	}

	return !ActiveHapticEffect.IsValid();
}
//@TODO - remove editor
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHapticBufferTest, "System.VR.All.Haptics.Buffer", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHapticBufferTest::RunTest(const FString&)
{
	bool bLoop = false;
	float Scale = 1.0f;

	//find test asset to play
	FString HapticEffectName = TEXT("/Game/Curves/QA_Haptic_StrongFivePulse.QA_Haptic_StrongFivePulse");
	UHapticFeedbackEffect_Base* HapticEffect = LoadObject<UHapticFeedbackEffect_Base>(nullptr, *HapticEffectName);
	if (HapticEffect)
	{
		TSharedPtr<struct FActiveHapticFeedbackEffect> ActiveHapticEffect;
		ActiveHapticEffect = MakeShareable(new FActiveHapticFeedbackEffect(HapticEffect, Scale, bLoop));

		uint32 MaxHand = ((uint32)EControllerHand::Right) + 1;
		for (uint32 HandIndex = 0; HandIndex < MaxHand; ++HandIndex)
		{
			//Buffer Check
			ADD_LATENT_AUTOMATION_COMMAND(FPlayBufferHapticLatentCommand((EControllerHand)HandIndex, ActiveHapticEffect));
		}
	}
	else
	{
		UE_LOG(LogHapticTest, Error, TEXT("Unable to find haptic effect %s"), *HapticEffectName);
	}
	return true;
}

/**
* Play Low Level Haptic Effect by Amp/Freq
*/
DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FPlayAmplitudeFrequencyHapticLatentCommand, EControllerHand, Hand, float, Amplitude, float, Frequency);
bool FPlayAmplitudeFrequencyHapticLatentCommand::Update()
{
	const int32 ControllerId = 0;

	IInputInterface* InputInterface = FSlateApplication::Get().GetInputInterface();
	if (InputInterface)
	{
		FHapticFeedbackValues HapticValues;
		HapticValues.Amplitude = Amplitude;
		HapticValues.Frequency = Frequency;

		InputInterface->SetHapticFeedbackValues(ControllerId, (int32)Hand, HapticValues);
	}

	return true;
}


//@TODO - remove editor
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAmplitudeFrequencyHapticTest, "System.VR.All.Haptics.AmplitudeAndFrequency", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAmplitudeFrequencyHapticTest::RunTest(const FString&)
{
	float ActiveDuration = 1.0f;
	uint32 MaxHand = ((uint32)EControllerHand::Right) + 1;
	for (uint32 HandIndex = 0; HandIndex < MaxHand; ++HandIndex)
	{
		//Amplitude Checks
		ADD_LATENT_AUTOMATION_COMMAND(FPlayAmplitudeFrequencyHapticLatentCommand((EControllerHand)HandIndex, .25f, 1.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(ActiveDuration));
		ADD_LATENT_AUTOMATION_COMMAND(FPlayAmplitudeFrequencyHapticLatentCommand((EControllerHand)HandIndex, .5f,  1.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(ActiveDuration));
		ADD_LATENT_AUTOMATION_COMMAND(FPlayAmplitudeFrequencyHapticLatentCommand((EControllerHand)HandIndex, .75f, 1.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(ActiveDuration));
		ADD_LATENT_AUTOMATION_COMMAND(FPlayAmplitudeFrequencyHapticLatentCommand((EControllerHand)HandIndex, 1.f, 1.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(ActiveDuration));

		//Frequency Checks
		ADD_LATENT_AUTOMATION_COMMAND(FPlayAmplitudeFrequencyHapticLatentCommand((EControllerHand)HandIndex, 1.0f, .25f));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(ActiveDuration));
		ADD_LATENT_AUTOMATION_COMMAND(FPlayAmplitudeFrequencyHapticLatentCommand((EControllerHand)HandIndex, 1.0f, .5f));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(ActiveDuration));
		ADD_LATENT_AUTOMATION_COMMAND(FPlayAmplitudeFrequencyHapticLatentCommand((EControllerHand)HandIndex, 1.0f, .75f));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(ActiveDuration));
		ADD_LATENT_AUTOMATION_COMMAND(FPlayAmplitudeFrequencyHapticLatentCommand((EControllerHand)HandIndex, 1.0f, 1.f));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(ActiveDuration));

		//turn back off!
		ADD_LATENT_AUTOMATION_COMMAND(FPlayAmplitudeFrequencyHapticLatentCommand((EControllerHand)HandIndex, 0.f, 0.0f));
	}


	return true;
}

#endif