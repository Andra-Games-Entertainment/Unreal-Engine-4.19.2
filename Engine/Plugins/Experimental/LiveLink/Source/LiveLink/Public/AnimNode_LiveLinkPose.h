// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "AnimGraphNode_Base.h"

class ILiveLinkClient;

#include "AnimNode_LiveLinkPose.generated.h"

USTRUCT()
struct LIVELINK_API FAnimNode_LiveLinkPose : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SourceData, meta = (PinShownByDefault))
	FName SubjectName;

	FAnimNode_LiveLinkPose() : LiveLinkClient(nullptr) {}

	// FAnimNode_Base interface
	virtual void Initialize(const FAnimationInitializeContext& Context) override;

	virtual void CacheBones(const FAnimationCacheBonesContext & Context) override {}

	virtual void Update(const FAnimationUpdateContext & Context) override {}

	virtual void Evaluate(FPoseContext& Output) override;
	// End of FAnimNode_Base interface

	void OnLiveLinkClientRegistered(const FName& Type, class IModularFeature* ModularFeature);
	void OnLiveLinkClientUnregistered(const FName& Type, class IModularFeature* ModularFeature);

private:

	ILiveLinkClient* LiveLinkClient;
};

UCLASS()
class UAnimGraphNode_LiveLinkPose : public UAnimGraphNode_Base
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_LiveLinkPose Node;

	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetMenuCategory() const;
	// End of UEdGraphNode
};