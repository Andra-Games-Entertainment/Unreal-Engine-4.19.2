// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FKey;

/** Immutable wrapper of shared pointer to a map */
template <class FKey, class FValue>
class TSharedMapView
{
	typedef TMap<FKey, FValue> FMap;
public:
	/** Default constructor - empty map */
	TSharedMapView() : Map(new FMap())
	{
	}

	/** Constructor from an existing map pointer */
	explicit TSharedMapView(TSharedRef<const FMap> InMap)
		: Map(InMap)
	{
	}

	/** Find a value by key (nullptr if not found) */
	const FValue* Find(typename FMap::KeyConstPointerType Key) const
	{
		return Map->Find(Key);
	}

	/** Find a value by key (abort if not found) */
	const FValue& FindChecked(typename FMap::KeyConstPointerType Key) const
	{
		return Map->FindChecked(Key);
	}

	/** Find a value by key (default value if not found) */
	FValue FindRef(typename FMap::KeyConstPointerType Key) const
	{
		return Map->FindRef(Key);
	}

	/** Determine whether a key is present in the map */
	bool Contains(typename FMap::KeyConstPointerType Key) const
	{
		return Map->Contains(Key);
	}

	/** Retrieve size of map */
	int32 Num() const
	{
		return Map->Num();
	}

	/** Populate an array with all the map's keys */
	template <class FAllocator>
	int32 GetKeys(TArray<FKey, FAllocator>& OutKeys) const
	{
		return Map->GetKeys(OutKeys);
	}

	/** Populate an array with all the map's keys */
	template <class FAllocator>
	int32 GenerateKeyArray(TArray<FKey, FAllocator>& OutKeys) const
	{
		return Map->GenerateKeyArray(OutKeys);
	}

	/** Populate an array with all the map's values */
	template <class FAllocator>
	int32 GenerateValueArray(TArray<FKey, FAllocator>& OutValues) const
	{
		return Map->GenerateValueArray(OutValues);
	}

	/** Iterate all key-value pairs */
	typename FMap::TConstIterator CreateConstIterator() const
	{
		return Map->CreateConstIterator();
	}

	/** Const access to the underlying map, mainly for taking a copy */
	const FMap& GetMap() const
	{
		return *Map;
	}

	/** Range for iterator access - DO NOT USE DIRECTLY */
	friend typename FMap::TConstIterator begin(const TSharedMapView& View)
	{
		return begin(*View.Map);
	}

	/** Range for iterator access - DO NOT USE DIRECTLY */
	friend typename FMap::TConstIterator end(const TSharedMapView& View)
	{
		return end(*View.Map);
	}

private:
	/** Pointer to map being wrapped */
	TSharedRef<const FMap> Map;
};

/** Helper function to wrap a TMap in a TSharedMapView */
template <class FKey, class FValue>
TSharedMapView<FKey, FValue> MakeSharedMapView(TMap<FKey, FValue> Map)
{
	return TSharedMapView<FKey, FValue>(MakeShareable(new TMap<FKey, FValue>(MoveTemp(Map))));
}
