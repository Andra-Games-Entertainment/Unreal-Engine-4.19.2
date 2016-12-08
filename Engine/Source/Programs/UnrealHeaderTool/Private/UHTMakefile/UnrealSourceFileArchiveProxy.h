// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "MakefileHelpers.h"
#include "HeaderProviderArchiveProxy.h"
#include "SimplifiedParsingClassInfo.h"

class FArchive;
class FUHTMakefile;
class FUnrealSourceFile;

/* See UHTMakefile.h for overview how makefiles work. */
struct FUnrealSourceFileArchiveProxy
{
	FUnrealSourceFileArchiveProxy(FUHTMakefile& UHTMakefile, FUnrealSourceFile* UnrealSourceFile);
	FUnrealSourceFileArchiveProxy() { }

	friend FArchive& operator<<(FArchive& Ar, FUnrealSourceFileArchiveProxy& UnrealSourceFileArchiveProxy);
	FUnrealSourceFile* CreateUnrealSourceFile(FUHTMakefile& UHTMakefile);
	void Resolve(FUnrealSourceFile* UnrealSourceFile, const FUHTMakefile& UHTMakefile);

	int32 ScopeIndex;
	int32 PackageIndex;

	FString Filename;
	FString GeneratedFileName;
	FString ModuleRelativePath;
	FString IncludePath;

	TArray<FHeaderProviderArchiveProxy> Includes;
	TArray<TPair<FSerializeIndex, FSimplifiedParsingClassInfo>> DefinedClassesWithParsingInfo;
	TArray<TPair<FSerializeIndex, uint8>> GeneratedCodeVersionsArchiveProxy;
};
