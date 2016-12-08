// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_DestructibleMesh.h"

#include "Editor/DestructibleMeshEditor/Public/DestructibleMeshEditorModule.h"

#include "Engine/DestructibleMesh.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_DestructibleMesh::GetSupportedClass() const
{
	return UDestructibleMesh::StaticClass();
}

void FAssetTypeActions_DestructibleMesh::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Mesh = Cast<UDestructibleMesh>(*ObjIt);
		if (Mesh != NULL)
		{
			FDestructibleMeshEditorModule& DestructibleMeshEditorModule = FModuleManager::LoadModuleChecked<FDestructibleMeshEditorModule>( "DestructibleMeshEditor" );
			TSharedRef< IDestructibleMeshEditor > NewDestructibleMeshEditor = DestructibleMeshEditorModule.CreateDestructibleMeshEditor( EToolkitMode::Standalone, EditWithinLevelEditor, Mesh );
		}
	}
}

#undef LOCTEXT_NAMESPACE
