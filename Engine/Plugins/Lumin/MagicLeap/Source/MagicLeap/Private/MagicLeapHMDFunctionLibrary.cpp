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

#include "MagicLeapHMDFunctionLibrary.h"
#include "MagicLeapHMD.h"
#include "Engine/Engine.h"
#include "ml_version.h"

static const FName MLDeviceName(TEXT("MagicLeap"));

// Internal helper.
static FMagicLeapHMD* GetMagicLeapHMD()
{
	IXRTrackingSystem* const XR = GEngine->XRSystem.Get();
	if (XR && (XR->GetSystemName() == MLDeviceName))
	{
		IHeadMountedDisplay* const HMD = XR->GetHMDDevice();
		if (HMD)
		{
			// we know it's a FMagicLeapHMD by the name match above
			return static_cast<FMagicLeapHMD*>(HMD);
		}
	}

	return nullptr;
}

void UMagicLeapHMDFunctionLibrary::SetBasePosition(const FVector& InBasePosition)
{
	FMagicLeapHMD* const HMD = GetMagicLeapHMD();
	if (HMD)
	{
		HMD->SetBasePosition(InBasePosition);
	}
}

void UMagicLeapHMDFunctionLibrary::SetBaseOrientation(const FQuat& InBaseOrientation)
{
	FMagicLeapHMD* const HMD = GetMagicLeapHMD();
	if (HMD)
	{
		HMD->SetBaseOrientation(InBaseOrientation);
	}
}

void UMagicLeapHMDFunctionLibrary::SetBaseRotation(const FRotator& InBaseRotation)
{
	FMagicLeapHMD* const HMD = GetMagicLeapHMD();
	if (HMD)
	{
		HMD->SetBaseRotation(InBaseRotation);
	}
}

void UMagicLeapHMDFunctionLibrary::SetFocusActor(const AActor* InFocusActor)
{
	FMagicLeapHMD* const HMD = GetMagicLeapHMD();
	if (HMD)
	{
		HMD->SetFocusActor(InFocusActor);
	}
}

int32 UMagicLeapHMDFunctionLibrary::GetMLSDKVersionMajor()
{
	return MLSDK_VERSION_MAJOR;
}

int32 UMagicLeapHMDFunctionLibrary::GetMLSDKVersionMinor()
{
	return MLSDK_VERSION_MINOR;
}

int32 UMagicLeapHMDFunctionLibrary::GetMLSDKVersionRevision()
{
	return MLSDK_VERSION_REVISION;
}

FString UMagicLeapHMDFunctionLibrary::GetMLSDKVersion()
{
	return TEXT(MLSDK_VERSION_NAME);
}

bool UMagicLeapHMDFunctionLibrary::IsRunningOnMagicLeapHMD()
{
#if PLATFORM_LUMIN
	return true;
#else
	return false;
#endif
}
