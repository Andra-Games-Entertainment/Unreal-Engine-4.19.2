// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LocalizationModule.h"
#include "LocalizationSettings.h"

ULocalizationTarget* FLocalizationModule::GetLocalizationTargetByName(FString TargetName, bool bIsEngineTarget)
{
	if (bIsEngineTarget)
	{
		ULocalizationTargetSet* EngineTargetSet = ULocalizationSettings::GetEngineTargetSet();
		for (ULocalizationTarget* Target : EngineTargetSet->TargetObjects)
		{
			if (Target->Settings.Name == TargetName)
			{
				return Target;
			}
		}
	}
	else
	{
		ULocalizationTargetSet* GameTargetSet = ULocalizationSettings::GetGameTargetSet();
		for (ULocalizationTarget* Target : GameTargetSet->TargetObjects)
		{
			if (Target->Settings.Name == TargetName)
			{
				return Target;
			}
		}
	}

	return nullptr;
}


IMPLEMENT_MODULE(FLocalizationModule, Localization);
