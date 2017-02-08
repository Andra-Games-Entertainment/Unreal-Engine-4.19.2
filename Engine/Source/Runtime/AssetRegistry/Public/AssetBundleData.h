// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PrimaryAssetId.h"
#include "AssetPtr.h"
#include "AssetBundleData.generated.h"

/** A struct representing a single AssetBundle */
USTRUCT()
struct ASSETREGISTRY_API FAssetBundleEntry
{
	GENERATED_USTRUCT_BODY()

	/** Asset this bundle is saved within. This is empty for global bundles, or in the saved bundle info */
	UPROPERTY()
	FPrimaryAssetId BundleScope;

	/** Specific name of this bundle, should be unique for a given scope */
	UPROPERTY()
	FName BundleName;

	/** List of string assets contained in this bundle */
	UPROPERTY()
	TArray<FStringAssetReference> BundleAssets;

	FAssetBundleEntry()	
	{}

	FAssetBundleEntry(const FAssetBundleEntry& OldEntry)
		: BundleScope(OldEntry.BundleScope), BundleName(OldEntry.BundleName), BundleAssets(OldEntry.BundleAssets)
	{

	}

	FAssetBundleEntry(const FPrimaryAssetId& InBundleScope, FName InBundleName)
		: BundleScope(InBundleScope), BundleName(InBundleName)
	{

	}

	/** Returns true if this represents a real entry */
	bool IsValid() { return BundleName != NAME_None; }

	/** Returns true if it has a valid scope, if false is a global entry or in the process of being created */
	bool IsScoped() { return BundleScope.IsValid(); }
};

/** A struct with a list of asset bundle entries. If one of these is inside a UObject it will get automatically exported as the asset registry tag AssetBundleData */
USTRUCT()
struct ASSETREGISTRY_API FAssetBundleData
{
	GENERATED_USTRUCT_BODY()

	/** List of bundles defined */
	UPROPERTY()
	TArray<FAssetBundleEntry> Bundles;

	/** Extract this out of an AssetData */
	bool SetFromAssetData(const class FAssetData& AssetData);

	/** Returns pointer to an entry with given Scope/Name */
	FAssetBundleEntry* FindEntry(const FPrimaryAssetId& SearchScope, FName SearchName);

	/** Adds or updates an entry with the given BundleName -> Path. Scope is empty and will be filled in later */
	void AddBundleAsset(FName BundleName, const FStringAssetReference& AssetPath);

	template< typename T >
	void AddBundleAsset(FName BundleName, const TAssetPtr<T>& AssetPtr)
	{
		AddBundleAsset(BundleName, AssetPtr.ToStringReference());
	}

	/** Adds multiple assets at once */
	void AddBundleAssets(FName BundleName, const TArray<FStringAssetReference>& AssetPaths);

	/** Resets the data to defaults */
	void Reset();

};
