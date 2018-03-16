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

#pragma once

#include "IInputInterface.h"
#include "IMagicLeapInputDevice.h"
#include "IHapticDevice.h"
#include "IMotionController.h"
#include "Misc/ScopeLock.h"

#include <ml_input.h>
#include "CoreMinimal.h"
#include "InputCoreTypes.h"

#include "MagicLeapControllerKeys.h"
#include "MagicLeapInputState.h"

//function to force the linker to include this cpp
void MagicLeapTestReferenceFunction();

class IMagicLeapTouchpadGestures;

/**
 * MagicLeap Motion Controller
 */
class FMagicLeapController : public IMagicLeapInputDevice, public IHapticDevice, public IMotionController
{
public:
	FMagicLeapController(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler);
	virtual ~FMagicLeapController();

	/** IMagicLeapInputDevice interface */
	virtual void Tick(float DeltaTime) override;
	virtual void SendControllerEvents() override;
	virtual void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override;
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;
	virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues &values) override;
	virtual class IHapticDevice* GetHapticDevice() override;
	virtual bool IsGamepadAttached() const override;
	virtual void Enable() override;
	virtual bool SupportsExplicitEnable() const override;
	virtual void Disable() override;

	/** IHapticDevice interface */
	virtual void SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values) override;
	virtual void GetHapticFrequencyRange(float& MinFrequency, float& MaxFrequency) const override;
	virtual float GetHapticAmplitudeScale() const override;

private:
	void InternalSetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values, bool bFromHapticInterface);
public:

	/** IMotionController interface */
	virtual bool GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const override;
	virtual ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const override;
	virtual FName GetMotionControllerDeviceTypeName() const override;

	/** FMagicLeapController interface */
	bool IsInputStateValid() const;

	void OnChar(uint32 CharUTF32);
	void OnKeyDown(MLKeyCode KeyCode);
	void OnKeyUp(MLKeyCode KeyCode);
	void OnTouchpadGestureStart(EControllerHand Hand, const MLInputControllerTouchpadGesture* TouchpadGesture);
	void OnTouchpadGestureContinue(EControllerHand Hand, const MLInputControllerTouchpadGesture* TouchpadGesture);
	void OnTouchpadGestureEnd(EControllerHand Hand, const MLInputControllerTouchpadGesture* TouchpadGesture);
	void OnButtonDown(uint8 ControllerID, MLInputControllerButton Button);
	void OnButtonUp(uint8 ControllerID, MLInputControllerButton Button);
	void OnControllerConnected(EControllerHand Hand);
	void OnControllerDisconnected(EControllerHand Hand);

	void SetControllerIsConnected(EControllerHand Hand, bool bConnected);

	bool GetControllerMapping(int32 ControllerIndex, EControllerHand& Hand) const;
	void InvertControllerMapping();
	EMLControllerType GetMLControllerType(EControllerHand Hand) const;
	void CalibrateControllerNow(EControllerHand Hand, const FVector& StartPosition, const FRotator& StartOrientation);

	bool PlayControllerLED(EControllerHand Hand, EMLControllerLEDPattern LEDPattern, EMLControllerLEDColor LEDColor, float DurationInSec);
	bool PlayControllerLEDEffect(EControllerHand Hand, EMLControllerLEDEffect LEDEffect, EMLControllerLEDSpeed LEDSpeed, EMLControllerLEDPattern LEDPattern, EMLControllerLEDColor LEDColor, float DurationInSec);
	bool PlayControllerHapticFeedback(EControllerHand Hand, EMLControllerHapticPattern HapticPattern, EMLControllerHapticIntensity Intensity);

	void RegisterTouchpadGestureReceiver(IMagicLeapTouchpadGestures* Receiver);
	void UnregisterTouchpadGestureReceiver(IMagicLeapTouchpadGestures* Receiver);

private:
	void UpdateTrackerData();
	void SendControllerEventsForHand(EControllerHand Hand);
	void AddKeys();
	void DebouncedButtonMessageHandler(bool NewButtonState, bool OldButtonState, const FName& ButtonName);
	const FName& MagicLeapButtonToUnrealButton(int32 ControllerID, MLInputControllerButton ml_button);
	const FName& MagicLeapTouchToUnrealThumbstickAxis(EControllerHand Hand, uint32 TouchIndex);
	const FName& MagicLeapTouchToUnrealThumbstickButton(EControllerHand Hand);
	const FName& MagicLeapTriggerToUnrealTriggerAxis(EControllerHand Hand);
	const FName& MagicLeapTriggerToUnrealTriggerKey(EControllerHand Hand);
	MLInputControllerFeedbackPatternLED UnrealToMLPatternLED(EMLControllerLEDPattern LEDPattern) const;
	MLInputControllerFeedbackEffectLED UnrealToMLEffectLED(EMLControllerLEDEffect LEDEffect) const;
	MLInputControllerFeedbackColorLED UnrealToMLColorLED(EMLControllerLEDColor LEDColor) const;
	MLInputControllerFeedbackEffectSpeedLED UnrealToMLSpeedLED(EMLControllerLEDSpeed LEDSpeed) const;
	MLInputControllerFeedbackPatternVibe UnrealToMLPatternVibe(EMLControllerHapticPattern HapticPattern) const;
	MLInputControllerFeedbackIntensity UnrealToMLHapticIntensity(EMLControllerHapticIntensity HapticIntensity) const;
	void InitializeInputCallbacks();

	TSharedPtr<FGenericApplicationMessageHandler> MessageHandler;
	int32 DeviceIndex;

	MLHandle InputTracker;
	MLInputControllerState InputState[MLInput_MaxControllers];
	MLInputControllerState OldInputState[MLInput_MaxControllers];
	MLInputControllerCallbacks InputControllerCallbacks;
	MLInputKeyboardCallbacks InputKeyboardCallbacks;

	bool bIsInputStateValid;
	bool bTriggerState;
	bool bTriggerKeyPressing;

	float triggerKeyIsConsideredPressed;
	float triggerKeyIsConsideredReleased;

	FTransform LeftControllerTransform;
	FTransform RightControllerTransform;

	FTransform LeftControllerCalibration;
	FTransform RightControllerCalibration;

	TMap<int32, EControllerHand> ControllerIDToHand;
	TMap<EControllerHand, int32> HandToControllerID;

	TMap<int32, FMagicLeapControllerState> ControllerIDToControllerState;

	struct FMagicLeapKeys
	{
		static const FKey MotionController_Left_Thumbstick_Z;
		static const FKey Left_MoveButton;
		static const FKey Left_AppButton;
		static const FKey Left_TapGesture;
		static const FKey Left_DoubleTapGesture;
		static const FKey Left_SwipeUpGesture;
		static const FKey Left_SwipeDownGesture;
		static const FKey Left_RadialScrollGesture;
		static const FKey Left_HomeButton;

		static const FKey MotionController_Right_Thumbstick_Z;
		static const FKey Right_MoveButton;
		static const FKey Right_AppButton;
		static const FKey Right_TapGesture;
		static const FKey Right_DoubleTapGesture;
		static const FKey Right_SwipeUpGesture;
		static const FKey Right_SwipeDownGesture;
		static const FKey Right_RadialScrollGesture;
		static const FKey Right_HomeButton;
	};

	struct FTouchpadGestureMap
	{
		EControllerHand Hand;
		MLInputControllerTouchpadGesture TouchpadGesture;
	};

	TArray<FTouchpadGestureMap> PendingTouchpadGestureStart;
	TArray<FTouchpadGestureMap> PendingTouchpadGestureContinue;
	TArray<FTouchpadGestureMap> PendingTouchpadGestureEnd;
	FCriticalSection TouchGestureCriticalSection;

	TArray<IMagicLeapTouchpadGestures*> TouchpadGestureReceivers;

	TArray<MLKeyCode> PendingKeyDowns;
	TArray<MLKeyCode> PendingKeyUps;
	TArray<uint32> PendingCharKeys;
	FCriticalSection KeyCriticalSection;

	struct FButtonMap
	{
		MLInputControllerButton Button;
		uint8 ControllerID;
		bool bPressed;
	};

	TArray<FButtonMap> PendingButtonStates;
	FCriticalSection ButtonCriticalSection;
};

DEFINE_LOG_CATEGORY_STATIC(LogMagicLeapController, Display, All);
