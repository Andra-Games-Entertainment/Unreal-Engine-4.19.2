// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VisualStudioSourceCodeAccessor.h"
#include "VisualStudioSourceCodeAccessorWrapper.h"

class FVisualStudioSourceCodeAccessModule : public IModuleInterface
{
public:
	FVisualStudioSourceCodeAccessModule();

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	FVisualStudioSourceCodeAccessor& GetAccessor();

private:
	void RegisterWrapper(FName Name, FText NameText, FText DescriptionText);

	TSharedRef<FVisualStudioSourceCodeAccessor> VisualStudioSourceCodeAccessor;
	TArray<TSharedRef<FVisualStudioSourceCodeAccessorWrapper>> Wrappers;
};