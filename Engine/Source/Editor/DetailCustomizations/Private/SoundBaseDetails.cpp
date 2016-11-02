// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "SoundBaseDetails.h"
#include "Sound/SoundBase.h"

TSharedRef<IDetailCustomization> FSoundBaseDetails::MakeInstance()
{
	return MakeShareable(new FSoundBaseDetails);
}

void FSoundBaseDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	if (!GetDefault<UEditorExperimentalSettings>()->bShowAudioMixerData)
	{
		TSharedRef<IPropertyHandle> Property = DetailBuilder.GetProperty("SoundSubmixObject", USoundBase::StaticClass());
		Property->MarkHiddenByCustomization();

		Property = DetailBuilder.GetProperty("SourceEffectChain", USoundBase::StaticClass());
		Property->MarkHiddenByCustomization();
	}
}
