// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MakefileHelpers.h"

class UObjectBase;
class FArchive;
class FUHTMakefile;

/* See UHTMakefile.h for overview how makefiles work. */
struct FObjectBaseArchiveProxy
{
	FObjectBaseArchiveProxy() { }
	FObjectBaseArchiveProxy(FUHTMakefile& UHTMakefile, const UObjectBase* ObjectBase);

	UObjectBase* CreateObjectBase(const FUHTMakefile& UHTMakefile) const;
	void Resolve(UObjectBase* ObjectBase, const FUHTMakefile& UHTMakefile) const;

	friend FArchive& operator<<(FArchive& Ar, FObjectBaseArchiveProxy& ObjectBaseArchiveProxy);
	
	static void AddReferencedNames(const UObjectBase* ObjectBase, FUHTMakefile& UHTMakefile);
	void PostConstruct(UObjectBase* ObjectBase) const;
	uint32 ObjectFlagsUint32;
	FSerializeIndex ClassIndex;
	FNameArchiveProxy Name;
	FSerializeIndex OuterIndex;
};
