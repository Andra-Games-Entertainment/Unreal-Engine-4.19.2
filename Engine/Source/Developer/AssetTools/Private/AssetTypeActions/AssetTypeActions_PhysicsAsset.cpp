// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_PhysicsAsset.h"
#include "Editor/PhAT/Public/PhATModule.h"
#include "PhysicsEngine/PhysicsAsset.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_PhysicsAsset::GetSupportedClass() const
{
	return UPhysicsAsset::StaticClass();
}

void FAssetTypeActions_PhysicsAsset::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto PhysicsAsset = Cast<UPhysicsAsset>(*ObjIt);
		if (PhysicsAsset != NULL)
		{
			
			IPhATModule* PhATModule = &FModuleManager::LoadModuleChecked<IPhATModule>( "PhAT" );
			PhATModule->CreatePhAT(Mode, EditWithinLevelEditor, PhysicsAsset);
		}
	}
}

#undef LOCTEXT_NAMESPACE
