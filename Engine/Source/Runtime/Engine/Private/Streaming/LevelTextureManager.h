// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
TextureInstanceManager.h: Definitions of classes used for texture streaming.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "StaticTextureInstanceManager.h"
#include "DynamicTextureInstanceManager.h"
#include "Templates/RefCounting.h"
#include "ContentStreaming.h"
#include "Engine/TextureStreamingTypes.h"
#include "Streaming/TextureStreamingHelpers.h"
#include "UObject/UObjectHash.h"

// The streaming data of a level.
class FLevelTextureManager
{
public:

	FLevelTextureManager(ULevel* InLevel, TextureInstanceTask::FDoWorkTask& AsyncTask) : Level(InLevel), bIsInitialized(false), StaticInstances(AsyncTask),  BuildStep(EStaticBuildStep::BuildTextureLookUpMap) {}

	ULevel* GetLevel() const { return Level; }

	// Remove the whole level
	void Remove(FDynamicTextureInstanceManager& DynamicManager, FRemovedTextureArray& RemovedTextures);

	// Invalidate a component reference.

	void RemoveActorReferences(const AActor* Actor)
	{
		StaticActorsWithNonStaticPrimitives.RemoveSingleSwap(Actor); 
		UnprocessedStaticActors.RemoveSingleSwap(Actor); 
	}

	void RemoveComponentReferences(const UPrimitiveComponent* Component, FRemovedTextureArray& RemovedTextures) 
	{ 
		// Check everywhere as the mobility can change in game.
		StaticInstances.Remove(Component, RemovedTextures); 
		DynamicComponents.RemoveSingleSwap(Component); 
		UnprocessedStaticComponents.RemoveSingleSwap(Component); 
		PendingInsertionStaticPrimitives.RemoveSingleSwap(Component); 
	}

	const FStaticTextureInstanceManager& GetStaticInstances() const { return StaticInstances; }

	float GetWorldTime() const;

	FORCEINLINE FTextureInstanceAsyncView GetAsyncView() { return FTextureInstanceAsyncView(StaticInstances.GetAsyncView()); }

	void IncrementalUpdate(FDynamicTextureInstanceManager& DynamicManager, FRemovedTextureArray& RemovedTextures, int64& NumStepsLeftForIncrementalBuild, float Percentage, bool bUseDynamicStreaming);

	uint32 GetAllocatedSize() const;

	bool IsInitialized() const { return bIsInitialized; }

private:

	ULevel* Level;

	bool bIsInitialized;

	FStaticTextureInstanceManager StaticInstances;

	/** The list of dynamic components contained in the level. Used to removed them from the FDynamicTextureInstanceManager on ::Remove. */
	TArray<const UPrimitiveComponent*> DynamicComponents;

	/** The static actors that had not only dynamic static components. */
	TArray<const AActor*> StaticActorsWithNonStaticPrimitives;

	/** Incremental build implementation. */

	enum class EStaticBuildStep : uint8
	{
		BuildTextureLookUpMap,
		GetActors,
		GetComponents,
		ProcessComponents,
		NormalizeLightmapTexelFactors,
		CompileElements,
		WaitForRegistration,
		Done,
	};

	// The current step of the incremental build.
	EStaticBuildStep BuildStep;
	// The actors / components left to be processed in ProcessComponents
	TArray<const AActor*> UnprocessedStaticActors;
	TArray<const UPrimitiveComponent*> UnprocessedStaticComponents;
	// The components that could not be processed by the incremental build.
	TArray<const UPrimitiveComponent*> PendingInsertionStaticPrimitives;
	// Reversed lookup for ULevel::StreamingTextureGuids.
	TMap<FGuid, int32> TextureGuidToLevelIndex;


	bool NeedsIncrementalBuild(int32 NumStepsLeftForIncrementalBuild) const;
	void IncrementalBuild(FStreamingTextureLevelContext& LevelContext, bool bForceCompletion, int64& NumStepsLeft);
};
