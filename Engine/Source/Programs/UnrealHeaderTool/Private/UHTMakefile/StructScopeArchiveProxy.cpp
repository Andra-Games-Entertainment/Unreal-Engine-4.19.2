// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StructScopeArchiveProxy.h"
#include "UnrealHeaderTool.h"
#include "Scope.h"
#include "UHTMakefile.h"

FArchive& operator<<(FArchive& Ar, FStructScopeArchiveProxy& StructScopeArchiveProxy)
{
	Ar << static_cast<FScopeArchiveProxy&>(StructScopeArchiveProxy);
	Ar << StructScopeArchiveProxy.StructIndex;

	return Ar;
}

FStructScopeArchiveProxy::FStructScopeArchiveProxy(const FUHTMakefile& UHTMakefile, const FStructScope* StructScope)
	: FScopeArchiveProxy(UHTMakefile, StructScope)
{
	StructIndex = UHTMakefile.GetStructIndex(StructScope->Struct);
}

void FStructScopeArchiveProxy::AddReferencedNames(const FStructScope* StructScope, FUHTMakefile& UHTMakefile)
{
	FScopeArchiveProxy::AddReferencedNames(StructScope, UHTMakefile);
}

FStructScope* FStructScopeArchiveProxy::CreateStructScope(const FUHTMakefile& UHTMakefile) const
{
	return new FStructScope(nullptr, nullptr);
}

void FStructScopeArchiveProxy::Resolve(FStructScope* StructScope, const FUHTMakefile& UHTMakefile) const
{
	FScopeArchiveProxy::Resolve(StructScope, UHTMakefile);
	StructScope->Struct = UHTMakefile.GetStructByIndex(StructIndex);

	FScope::ScopeMap.Add(StructScope->Struct, StructScope->DoesSharedInstanceExist() ? StructScope->AsShared() : MakeShareable(StructScope));
}
