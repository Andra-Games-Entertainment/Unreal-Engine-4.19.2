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

#include "GoogleVRControllerPrivate.h"
#include "IInputDevice.h"
#include "IMotionController.h"
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
#include "gvr_arm_model.h"
#endif
#include "Classes/GoogleVRControllerEventManager.h"
#include "Classes/GoogleVRControllerFunctionLibrary.h"

#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
using namespace gvr;
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGoogleVRController, Log, All);

/** Total number of controllers in a set */
#define CONTROLLERS_PER_PLAYER	2

/** Controller axis mappings. */
#define DOT_45DEG		0.7071f

namespace AndroidControllerKeyNames
{
	const FGamepadKeyNames::Type AndroidMenu("Android_Menu");
	const FGamepadKeyNames::Type AndroidBack("Android_Back");
	const FGamepadKeyNames::Type AndroidVolumeUp("Android_Volume_Up");
	const FGamepadKeyNames::Type AndroidVolumeDown("Android_Volume_Down");
}

namespace GoogleVRControllerKeyNames
{
	const FGamepadKeyNames::Type Touch0("Steam_Touch_0");
}

class FGoogleVRController : public IInputDevice, public IMotionController
{
public:

#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
	FGoogleVRController(gvr::ControllerApi* pControllerAPI, const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler);
#else
	FGoogleVRController(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler);
#endif
	virtual ~FGoogleVRController();

public:

	struct EGoogleVRControllerButton
	{
		enum Type
		{
			ApplicationMenu,
			TouchPadLeft,
			TouchPadUp,
			TouchPadRight,
			TouchPadDown,
			System,
			TriggerPress,
			Grip,
			TouchPadPress,
			TouchPadTouch,

			/** Max number of controller buttons.  Must be < 256 */
			TotalButtonCount
		};
	};

public: // Helper Functions

	/** Called beforeg application enters background */
	void ApplicationPauseDelegate();

	/** Called after application resumes */
	void ApplicationResumeDelegate();

	/** Polls the controller state */
	void PollController(float DeltaTime);

	/** Processes the controller buttons */
	void ProcessControllerButtons();

	/** Processes the controller events */
	void ProcessControllerEvents();

	/** Checks if the controller is available */
	bool IsAvailable() const;

	int GetGVRControllerHandedness() const;

	EGoogleVRControllerState GetControllerState() const;

	FVector ConvertGvrVectorToUnreal(float x, float y, float z, float WorldToMetersScale) const;

	FQuat ConvertGvrQuaternionToUnreal(float w, float x, float y, float z) const;

	/** Checks if the controller battery is charging. */
	bool GetBatteryCharging();

	/** Returns an approximate battery level. */
	EGoogleVRControllerBatteryLevel GetBatteryLevel();

	/** Returns the time stamp the battery information was last updated. */
	int64_t GetLastBatteryTimestamp();

public: // Arm Model

	bool GetUseArmModel() const;
	void SetUseArmModel(bool bNewUseArmModel);
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
	gvr_arm_model::Controller& GetArmModelController();
#endif

public:	// IInputDevice

	/** Tick the interface (e.g. check for new controllers) */
	virtual void Tick(float DeltaTime);

	/** Poll for controller state and send events if needed */
	virtual void SendControllerEvents();

	/** Set which MessageHandler will get the events from SendControllerEvents. */
	virtual void SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler);

	/** Exec handler to allow console commands to be passed through for debugging */
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar);

	/**
	* IForceFeedbackSystem pass through functions
	*/
	virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value);
	virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues &values);

public: // IMotionController
	/**
	* Returns the calibration-space orientation of the requested controller's hand.
	*
	* @param ControllerIndex	The Unreal controller (player) index of the contoller set
	* @param DeviceHand		Which hand, within the controller set for the player, to get the orientation and position for
	* @param OutOrientation	(out) If tracked, the orientation (in calibrated-space) of the controller in the specified hand
	* @param OutPosition		(out) If tracked, the position (in calibrated-space) of the controller in the specified hand
	* @return					True if the device requested is valid and tracked, false otherwise
	*/
	virtual bool GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const;

	/**
	* Returns the tracking status (e.g. not tracked, intertial-only, fully tracked) of the specified controller
	*
	* @return	Tracking status of the specified controller, or ETrackingStatus::NotTracked if the device is not found
	*/
	virtual ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const;

	/** Cached controller info */
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
	gvr::ControllerState CachedControllerState;
#endif

	float  GetWorldToMetersScale() const;

private:


#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
	/** GVR Controller client reference */
	gvr::ControllerApi *pController;

	/** Capture Button Press states */
	bool LastButtonStates[EGoogleVRControllerButton::TotalButtonCount];

	/** Button mappings */
	FGamepadKeyNames::Type Buttons[CONTROLLERS_PER_PLAYER][EGoogleVRControllerButton::TotalButtonCount];
#endif
	bool bControllerReadyToPollState;

	/** handler to send all messages to */
	TSharedRef<FGenericApplicationMessageHandler> MessageHandler;

	// TODO: Set controller handedness based on the value from gvr.
	// When does the gvr value become available? Should we just poll for it?
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
	gvr_arm_model::Controller ArmModelController;
#endif
	bool bUseArmModel;
	EGoogleVRControllerState CurrentControllerState;

#if GOOGLEVRCONTROLLER_SUPPORTED_EMULATOR_PLATFORMS
	FRotator BaseEmulatorOrientation;
#endif

	/** Last Orientation used */
	mutable FRotator LastOrientation;
};
