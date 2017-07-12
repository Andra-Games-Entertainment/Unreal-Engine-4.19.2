// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IOSTargetPlatform.h"
#include "Interfaces/ITargetPlatformModule.h"

/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* Singleton = NULL;


/**
 * Module for iOS as a target platform
 */
class FIOSTargetPlatformModule : public ITargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	~FIOSTargetPlatformModule()
	{
		Singleton = NULL;
	}


public:

	// Begin ITargetPlatformModule interface

	virtual ITargetPlatform* GetTargetPlatform()
	{
		if (Singleton == NULL)
		{
			Singleton = new FIOSTargetPlatform(false);
		}

		return Singleton;
	}

	// End ITargetPlatformModule interface


public:

	// Begin IModuleInterface interface
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
	// End IModuleInterface interface
};

IMPLEMENT_MODULE( FIOSTargetPlatformModule, IOSTargetPlatform);
