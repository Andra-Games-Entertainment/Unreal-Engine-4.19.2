// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ActorFactories/ActorFactoryStaticMesh.h"
#include "ActorFactoryInteractiveFoliage.generated.h"

class FAssetData;

UCLASS(MinimalAPI, config=Editor)
class UActorFactoryInteractiveFoliage : public UActorFactoryStaticMesh
{
	GENERATED_UCLASS_BODY()


	//~ Begin UActorFactory Interface
	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg ) override { return false; };
	//~ End UActorFactory Interface
};



