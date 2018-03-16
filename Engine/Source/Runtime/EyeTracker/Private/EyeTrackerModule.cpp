// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IEyeTrackerModule.h"
#include "ModuleManager.h"


class FEyeTrackerModule : public IEyeTrackerModule
{
	virtual TSharedPtr< class IEyeTracker, ESPMode::ThreadSafe > CreateEyeTracker()
	{
		TSharedPtr<IEyeTracker, ESPMode::ThreadSafe> DummyVal = nullptr;
		return DummyVal;
	}

	FString GetModuleKeyName() const
	{
		return FString(TEXT("Default"));
	}

	virtual bool IsEyeTrackerConnected() const override { return false; }
};

IMPLEMENT_MODULE(FEyeTrackerModule, EyeTracker);

