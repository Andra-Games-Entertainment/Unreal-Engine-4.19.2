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

#include "MagicLeapGestureTypes.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InputCoreTypes.h"
#include "MagicLeapGesturesFunctionLibrary.generated.h"

UCLASS(ClassGroup = MagicLeap)
class MAGICLEAPGESTURES_API UMagicLeapGesturesFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	  Transform of the center of the hand.  Approximately the center of the palm.

	  Note that this returns a transform in the Tracking space. To get the transform in Unreal's
	  world space, use the MotioController component as a child of the XRPawn with hand set to EControllerHand::Special_1
	  for the left hand center and EControllerHand::Special_2 for the right hand center.

	  @param Hand Hand to query the hand center transform for. Only Left and Right hand are supported.
	  @param HandCenter Output parameter containing the position and orientation of the given hand.
	  @return true if the output param was populated with a valid value, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandGestures|MagicLeap")
	static bool GetHandCenter(EControllerHand Hand, FTransform& HandCenter);

	/**
	  Transform of Keypoint 0.
	  For the Finger, Pinch, L, OK, and C gestures this is the index finger tip.
	  For the Fist gesture this is the first knuckle of the middle finger.
	  For the Thumb gesture this is the thumb tip.
	  For the Open Hand Back gesture this is the middle finger tip.

	  Note that this returns a transform in the Tracking space. To get the transform in Unreal's
	  world space, use the MotioController component as a child of the XRPawn with hand set to EControllerHand::Special_3
	  for the left hand pointer and EControllerHand::Special_4 for the right hand pointer.

	  @param Hand Hand to query the hand center transform for. Only Left and Right hand are supported.
	  @param HandCenter Output parameter containing the position and orientation.
	  @return true if the output param was populated with a valid value, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandGestures|MagicLeap")
	static bool GetHandPointer(EControllerHand Hand, FTransform& Pointer);

	/**
	  Transform of Keypoint 1.
	  Fist and OpenHandBack do not have this, so we return HandCenter.
	  For the C and L gestures this is the thumb tip.  
	  For Finger, OK, and Pinch this is the first knuckle of the index finger. 
	  For Thumb this is the first knuckle of the thumb.

	  Note that this returns a transform in the Tracking space. To get the transform in Unreal's
	  world space, use the MotioController component as a child of the XRPawn with hand set to EControllerHand::Special_5
	  for the left hand secondary and EControllerHand::Special_6 for the right hand secondary.

	  @param Hand Hand to query the hand center transform for. Only Left and Right hand are supported.
	  @param HandCenter Output parameter containing the position and orientation.
	  @return true if the output param was populated with a valid value, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandGestures|MagicLeap")
	static bool GetHandSecondary(EControllerHand Hand, FTransform& Secondary);

	/**
	  Normalized position of the center of the given hand. This can be used to detect and warn the users that the hand is out of the gesture detection frame.

	  @param Hand Hand to query the normalized hand center position for. Only Left and Right hand are supported.
	  @param HandCenterNormalized Output paramter containing the normalized position of the given hand.
	  @return true if the output param was populated with a valid value, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandGestures|MagicLeap")
	static bool GetHandCenterNormalized(EControllerHand Hand, FVector& HandCenterNormalized);

	/**
	  List of keypoints detected on the given hand.

	  Note that this returns a transform in the Tracking space. To get the transform in Unreal's
	  world space, use the MotioController component as a child of the XRPawn with hand set to the following.
	  Special_3 - Left keypoint 0
	  Special_5 - Left keypoint 1
	  Special_4 - Right keypoint 0
	  Special_6 - Right keypoint 1

	  @param Hand Hand to query the keypoints for. Only Left and Right hand are supported.
	  @param Keypoints Output paramter containing transforms of the keypoints detected on the given hand.
	  @return true if the output param was populated with a valid value, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandGestures|MagicLeap")
	static bool GetGestureKeypoints(EControllerHand Hand, TArray<FTransform>& Keypoints);

	/**
	  Enables and disables the gestures to be detected by the gesture recognition system.

	  @param StaticGesturesToActivate List of static gestures to be detected by the system.
	  @param KeypointsFilterLevel Filtering for the keypoints and hand centers.
	  @param GestureFilterLevel Filtering for the static gesture recognition.
	  @param HandSwitchingFilterLevel Filtering for if the left or right hand is present.
	  @return true if the configuration was set successfully.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandGestures|MagicLeap")
	static bool SetConfiguration(const TArray<EStaticGestures>& StaticGesturesToActivate, EGestureKeypointsFilterLevel KeypointsFilterLevel = EGestureKeypointsFilterLevel::NoFilter, EGestureRecognitionFilterLevel GestureFilterLevel = EGestureRecognitionFilterLevel::NoFilter, EGestureRecognitionFilterLevel HandSwitchingFilterLevel = EGestureRecognitionFilterLevel::NoFilter);

	/**
	  Gets the list of static and dynamic gestures currently set to be identified by the gesture recognition system.

	  @param StaticGesturesToActivate Output parameter to list the static gestures that can be detected by the system.
	  @param KeypointsFilterLevel Filtering for the keypoints and hand centers.
	  @param GestureFilterLevel Filtering for the static gesture recognition.
	  @param HandSwitchingFilterLevel Filtering for if the left or right hand is present.
	  @return true if the output params were populated with a valid value, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandGestures|MagicLeap")
	static bool GetConfiguration(TArray<EStaticGestures>& ActiveStaticGestures, EGestureKeypointsFilterLevel& KeypointsFilterLevel, EGestureRecognitionFilterLevel& GestureFilterLevel, EGestureRecognitionFilterLevel& HandSwitchingFilterLevel);

	/**
	  Sets the minimum gesture confidence to filter out the detected static gesture.

	  @param Gesture The gesture to set the confidence threshold for.
	  @param Confidence The gesture confidence threshold.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandGestures|MagicLeap")
	static void SetStaticGestureConfidenceThreshold(EStaticGestures Gesture, float Confidence);

	/**
	  Gets the minimum gesture confidence used to filter out the detected static gesture.

	  @param Gesture The gesture to get the confidence threshold for.
	  @return The gesture confidence threshold.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandGestures|MagicLeap")
	static float GetStaticGestureConfidenceThreshold(EStaticGestures Gesture);

	/**
	  The confidence level of the current gesture being performed by the given hand.
	  Value is between [0, 1], 0 is low, 1 is high degree of confidence. For a NoHand, the confidence is always set to 1.

	  @param Hand Hand to query the gesture confidence value for. Only Left and Right hand are supported.
	  @param Confidence Output parameter containing the confidence value for the given hand's gesture.
	  @return true if the output param was populated with a valid value, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandGestures|MagicLeap")
	static bool GetHandGestureConfidence(EControllerHand Hand, float& Confidence);
	
	/**
	  The current gesture being performed by the given hand.

	  @param Hand Hand to query the gesture for. Only Left and Right hand are supported.
	  @param Gesture Output parameter containing the given hand's gesture, or NoHand if there isn't one or the system isnt working now.
	  @return true if the output param was populated with a valid value, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandGestures|MagicLeap")
	static bool GetCurrentGesture(EControllerHand Hand, EStaticGestures& Gesture);
};
