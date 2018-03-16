// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LuminTargetPlatform.h: Declares the FLuminTargetPlatform class.
=============================================================================*/

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Delegates/IDelegateInstance.h"
#include "Containers/Map.h"
#include "Delegates/Delegate.h"
#include "Containers/Ticker.h"
#include "Misc/ScopeLock.h"
#include "Common/TargetPlatformBase.h"
#include "Android/AndroidProperties.h"
#include "AndroidTargetPlatform.h"

#if WITH_ENGINE
#include "Internationalization/Text.h"
#include "StaticMeshResources.h"
#endif // WITH_ENGINE

class FTargetDeviceId;
class ILuminDeviceDetection;
class UTextureLODSettings;
enum class ETargetPlatformFeatures;

/**
 * FSeakTargetPlatform, abstraction for cooking Lumin platforms
 */
class FLuminTargetPlatform : public FAndroidTargetPlatform<FAndroidPlatformProperties>
{
public:

	/**
	 * Default constructor.
	 */
	FLuminTargetPlatform();

	/**
	 * Destructor
	 */
	virtual ~FLuminTargetPlatform();

public:

	// Begin ITargetPlatform interface

	virtual FString PlatformName() const override
	{
		return TEXT("Lumin");
	}

	virtual bool IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const override;

	virtual int32 CheckRequirements(const FString& ProjectPath, bool bProjectHasCode, FString& OutTutorialPath, FString& OutDocumentationPath, FText& CustomizedLogMessage) const override;

#if WITH_ENGINE

	virtual void GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const override;

	virtual void GetTextureFormats( const UTexture* InTexture, TArray<FName>& OutFormats ) const override;
	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override;
#endif //WITH_ENGINE

	virtual void GetBuildProjectSettingKeys(FString& OutSection, TArray<FString>& InBoolKeys, TArray<FString>& InIntKeys, TArray<FString>& InStringKeys) const override
	{
		OutSection = TEXT("/Script/LuminRuntimeSettings.LuminRuntimeSettings");
	}


	// End ITargetPlatform interface

	virtual bool SupportsDesktopRendering() const override;
	virtual bool SupportsMobileRendering() const override;
	virtual void InitializeDeviceDetection() override;

protected:
	// Holds the Engine INI settings, for quick use.
	FConfigFile EngineSettings;

};

