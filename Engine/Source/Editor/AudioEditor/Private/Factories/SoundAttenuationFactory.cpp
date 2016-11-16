// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AudioEditorPrivatePCH.h"
#include "Factories/SoundAttenuationFactory.h"

USoundAttenuationFactory::USoundAttenuationFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = USoundAttenuation::StaticClass();
	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* USoundAttenuationFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<USoundAttenuation>(InParent, Name, Flags);
}


