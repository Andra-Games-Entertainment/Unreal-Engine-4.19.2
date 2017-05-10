// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/StringClassReference.h"
#include "Templates/Casts.h"


#include "Misc/StringReferenceTemplates.h"

bool FStringClassReference::SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FArchive& Ar)
{
	struct UClassTypePolicy
	{
		typedef UClass Type;
		static const FName FORCEINLINE GetTypeName() { return NAME_ClassProperty; }
	};

	FString Path = ToString();

	bool bReturn = SerializeFromMismatchedTagTemplate<UClassTypePolicy>(Path, Tag, Ar);

	if (Ar.IsLoading())
	{
		SetPath(MoveTemp(Path));
		PostLoadPath();
	}

	return bReturn;
}

UClass* FStringClassReference::ResolveClass() const
{
	return dynamic_cast<UClass*>(ResolveObject());
}

FStringClassReference FStringClassReference::GetOrCreateIDForClass(const UClass *InClass)
{
	check(InClass);
	return FStringClassReference(InClass);
}
