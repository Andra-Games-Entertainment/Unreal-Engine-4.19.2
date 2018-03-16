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

#include "MagicLeapPrivileges.h"
#include "IMagicLeapPrivilegesPlugin.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "MagicLeapPluginUtil.h"
#include "MagicLeapSDKDetection.h"

#include "ml_privileges.h"

DEFINE_LOG_CATEGORY_STATIC(LogMagicLeapPrivileges, Display, All);

class FMagicLeapPrivilegesPlugin : public IMagicLeapPrivilegesPlugin
{
public:
	void StartupModule() override
	{
		IModuleInterface::StartupModule();
		APISetup.Startup();
		APISetup.LoadDLL(TEXT("ml_privileges"));
	}

	void ShutdownModule() override
	{
		APISetup.Shutdown();
		IModuleInterface::ShutdownModule();
	}

private:
	FMagicLeapAPISetup APISetup;
};

IMPLEMENT_MODULE(FMagicLeapPrivilegesPlugin, MagicLeapPrivileges);

//////////////////////////////////////////////////////////////////////////

MLPrivilegeID UnrealToMLPrivilege(EMagicLeapPrivilege Privilege)
{
// TODO: We need to get rid of any hand-mapping of these enums. In the meantime,
// the macro is to make it easier to keep it in step with the header - rmobbs
#define PRIVCASE(x) case EMagicLeapPrivilege::x: { return MLPrivilegeID_##x; }
	switch(Privilege)
	{
		PRIVCASE(TestReality)
		PRIVCASE(TestSensitive)
		PRIVCASE(TestAutogranted)
		PRIVCASE(AddressBookRead)
		PRIVCASE(AddressBookWrite)
		PRIVCASE(AudioRecognizer)
		PRIVCASE(AudioRender)
		PRIVCASE(AudioSettings)
		PRIVCASE(BatteryInfo)
		PRIVCASE(CalendarRead)
		PRIVCASE(CalendarWrite)
		PRIVCASE(CameraCapture)
		PRIVCASE(DenseMap)
		PRIVCASE(EmailSend)
		PRIVCASE(Eyetrack)
		PRIVCASE(Headpose)
		PRIVCASE(InAppPurchase)
		PRIVCASE(Location)
		PRIVCASE(AudioCaptureMic)
		PRIVCASE(MMPlayback)
		PRIVCASE(DrmCertificates)
		PRIVCASE(Occlusion)
		PRIVCASE(ScreenCapture)
		PRIVCASE(Internet)
		PRIVCASE(GraphicsClient)
		PRIVCASE(AudioCaptureMixed)
		PRIVCASE(IdentityRead)
		PRIVCASE(IdentityModify)
		PRIVCASE(BackgroundDownload)
		PRIVCASE(BackgroundUpload)
		PRIVCASE(MediaDrm)
		PRIVCASE(Media)
		PRIVCASE(MediaMetadata)
		PRIVCASE(PowerInfo)
		PRIVCASE(AudioCaptureVirtual)
		PRIVCASE(CalibrationRigModelRead)
		PRIVCASE(NetworkServer)
		PRIVCASE(LocalAreaNetwork)
		PRIVCASE(Input)
		PRIVCASE(VoiceInput)
		PRIVCASE(ConnectBackgroundMusicService)
		PRIVCASE(RegisterBackgroundMusicService)
		PRIVCASE(NormalNotificationsUsage)
		PRIVCASE(MusicService)
		default:
			UE_LOG(LogMagicLeapPrivileges, Error, TEXT("Unmapped privilege %d"), static_cast<int32>(Privilege));
			return MLPrivilegeID_Invalid;
	}
}

UMagicLeapPrivileges::UMagicLeapPrivileges()
: bPrivilegeServiceStarted(false)
{
#if !PLATFORM_MAC
	ML_FUNCTION_WRAPPER(bPrivilegeServiceStarted = MLPrivilegesInit());
	if (!bPrivilegeServiceStarted)
	{
		// Setting log level to Error caused cook error.
		UE_LOG(LogMagicLeapPrivileges, Warning, TEXT("Error initializing privilege service."));
	}
#endif
}

UMagicLeapPrivileges::~UMagicLeapPrivileges()
{
}

void UMagicLeapPrivileges::FinishDestroy()
{
	if (bPrivilegeServiceStarted)
	{
		MLPrivilegesDestroy();
		bPrivilegeServiceStarted = false;
	}

	Super::FinishDestroy();
}

bool UMagicLeapPrivileges::CheckPrivilege(EMagicLeapPrivilege Privilege)
{
	return InitializePrivileges() ? MLPrivilegesCheckPrivilege(UnrealToMLPrivilege(Privilege)) : false;
}

bool UMagicLeapPrivileges::RequestPrivilege(EMagicLeapPrivilege Privilege)
{
	return InitializePrivileges() ? MLPrivilegesRequestPrivilege(UnrealToMLPrivilege(Privilege)) : false;
}

bool UMagicLeapPrivileges::InitializePrivileges()
{
	if (!bPrivilegeServiceStarted)
	{
		bPrivilegeServiceStarted = MLPrivilegesInit();
		if (!bPrivilegeServiceStarted)
		{
			UE_LOG(LogMagicLeapPrivileges, Warning, TEXT("Error initializing privilege service."));
		}
	}
	return bPrivilegeServiceStarted;
}
