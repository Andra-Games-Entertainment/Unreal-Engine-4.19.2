// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "MakefileHelpers.h"
#include "UnrealTypeDefinitionInfo.h"

class FUHTMakefile;
class UField;
class FArchive;
class FUnrealTypeDefinitionInfo;

/* See UHTMakefile.h for overview how makefiles work. */
struct FTypeDefinitionInfoMapArchiveProxy
{
	FTypeDefinitionInfoMapArchiveProxy(FUHTMakefile& UHTMakefile, TArray<TPair<UField*, FUnrealTypeDefinitionInfo*>>& TypeDefinitionInfoPairs);
	FTypeDefinitionInfoMapArchiveProxy() { }

	TArray<TPair<FSerializeIndex, int32>> TypeDefinitionInfoIndexes;

	friend FArchive& operator<<(FArchive& Ar, FTypeDefinitionInfoMapArchiveProxy& FileScopeArchiveProxy)
	{
		Ar << FileScopeArchiveProxy.TypeDefinitionInfoIndexes;

		return Ar;
	}

	void ResolveIndex(FUHTMakefile& UHTMakefile, int32 Index);
	void ResolveClassIndex(FUHTMakefile& UHTMakefile, int32 Index);
};
