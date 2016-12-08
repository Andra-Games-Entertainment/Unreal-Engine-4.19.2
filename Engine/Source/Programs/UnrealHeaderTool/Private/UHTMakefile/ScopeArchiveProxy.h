// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "MakefileHelpers.h"

class FArchive;
class FUHTMakefile;
class FScope;

/* See UHTMakefile.h for overview how makefiles work. */
struct FScopeArchiveProxy
{
	FScopeArchiveProxy(const FUHTMakefile& UHTMakefile, const FScope* Scope);
	FScopeArchiveProxy() { }

	static void AddReferencedNames(const FScope* Scope, FUHTMakefile& UHTMakefile);

	friend FArchive& operator<<(FArchive& Ar, FScopeArchiveProxy& ScopeArchiveProxy);
	void Resolve(FScope* Scope, const FUHTMakefile& UHTMakefile) const;

	int32 ParentIndex;
	TArray<TPair<FNameArchiveProxy, FSerializeIndex>> TypeMap;
};
