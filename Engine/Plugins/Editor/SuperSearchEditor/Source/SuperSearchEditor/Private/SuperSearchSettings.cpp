// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SuperSearchSettings.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "SuperSearchModule.h"


class FSuperSearchEditorModule : public FDefaultModuleImpl
{
public:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(FSuperSearchEditorModule, SuperSearchEditor);

void USuperSearchSettings::PostInitProperties()
{
	Super::PostInitProperties();

	FSuperSearchModule::Get().SetSearchEngine(SearchEngine);
}

void USuperSearchSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if ( Name == GET_MEMBER_NAME_CHECKED(USuperSearchSettings, SearchEngine) )
	{
		FSuperSearchModule::Get().SetSearchEngine(SearchEngine);
	}
}
