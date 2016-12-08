// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IFuncTestManager.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class FFunctionalTestingModule : public IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Gets the debugger singleton or returns NULL */
	static class IFuncTestManager* Get()
	{
		FFunctionalTestingModule& Module = FModuleManager::Get().LoadModuleChecked<FFunctionalTestingModule>("FunctionalTesting");
		return Module.GetSingleton();
	}

private:
	void OnModulesChanged(FName Module, EModuleChangeReason Reason);

	class IFuncTestManager* GetSingleton() const 
	{ 
		return Manager.Get(); 
	}

private:
	TSharedPtr<class IFuncTestManager> Manager;
};
