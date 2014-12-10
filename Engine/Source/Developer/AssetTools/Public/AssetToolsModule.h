// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "IAssetTypeActions.h"
#include "IAssetTools.h"

class FAssetToolsModule : public IModuleInterface
{
public:
	virtual void StartupModule();
	virtual void ShutdownModule();

	virtual IAssetTools& Get() const;

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FAssetToolsModule& GetModule()
	{
		return FModuleManager::LoadModuleChecked< FAssetToolsModule >("AssetTools");
	}

private:
	IAssetTools* AssetTools;
	class FAssetToolsConsoleCommands* ConsoleCommands;
};
