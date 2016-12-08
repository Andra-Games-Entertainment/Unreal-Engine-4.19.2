// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Player/AndroidMediaPlayer.h"
#include "IAndroidMediaModule.h"
#include "Modules/ModuleManager.h"



#define LOCTEXT_NAMESPACE "FAndroidMediaModule"


class FAndroidMediaModule
	: public IAndroidMediaModule
{
public:

	//~ IAndroidMediaModule interface

	virtual TSharedPtr<IMediaPlayer> CreatePlayer() override
	{
		if (!IsSupported())
		{
			return nullptr;
		}

		return MakeShareable(new FAndroidMediaPlayer());
	}

public:

	//~ IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }

protected:

	/**
	 * Check whether media is supported on the running device.
	 *
	 * @return true if media is supported, false otherwise.
	 */
	bool IsSupported()
	{
		return (FAndroidMisc::GetAndroidBuildVersion() >= 14);
	}
};


IMPLEMENT_MODULE(FAndroidMediaModule, AndroidMedia)

#undef LOCTEXT_NAMESPACE
