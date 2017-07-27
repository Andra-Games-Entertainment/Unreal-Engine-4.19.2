// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DestructibleChunkParamsProxy.h"
#include "IDestructibleMeshEditor.h"
#include "ApexDestructibleAssetImport.h"
#include "DestructibleMesh.h"

UDestructibleChunkParamsProxy::UDestructibleChunkParamsProxy(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{

}

void UDestructibleChunkParamsProxy::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent )
{
	check(DestructibleMesh != NULL && DestructibleMesh->FractureSettings != NULL);

	if (DestructibleMesh->FractureSettings->ChunkParameters.Num() > ChunkIndex)
	{
		DestructibleMesh->FractureSettings->ChunkParameters[ChunkIndex] = ChunkParams;
	}

#if WITH_APEX
	BuildDestructibleMeshFromFractureSettings(*DestructibleMesh, NULL);
#endif
	DestructibleMeshEditorPtr.Pin()->RefreshViewport();
}
