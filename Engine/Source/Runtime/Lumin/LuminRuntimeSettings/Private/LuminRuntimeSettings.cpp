// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LuminRuntimeSettings.h"
#include "ConfigCacheIni.h"
#include "CoreGlobals.h"
#include <Paths.h>

#if WITH_EDITOR

void ULuminRuntimeSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	GConfig->Flush(1);
}

#endif