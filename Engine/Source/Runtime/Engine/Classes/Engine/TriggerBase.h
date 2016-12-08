// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "TriggerBase.generated.h"

class UBillboardComponent;
class UShapeComponent;

/** An actor used to generate collision events (begin/end overlap) in the level. */
UCLASS(ClassGroup=Common, abstract, ConversionRoot, MinimalAPI)
class ATriggerBase : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	/** Shape component used for collision */
	UPROPERTY(Category = TriggerBase, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UShapeComponent* CollisionComponent;

	/** Billboard used to see the trigger in the editor */
	UPROPERTY()
	UBillboardComponent* SpriteComponent;

public:
	/** Returns CollisionComponent subobject **/
	ENGINE_API UShapeComponent* GetCollisionComponent() const { return CollisionComponent; }
	/** Returns SpriteComponent subobject **/
	ENGINE_API UBillboardComponent* GetSpriteComponent() const { return SpriteComponent; }
};



