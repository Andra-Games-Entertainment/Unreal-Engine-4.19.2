// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimData/BoneMaskFilter.h"
#include "AnimNode_LayeredBoneBlend.generated.h"

// Layered blend (per bone); has dynamic number of blendposes that can blend per different bone sets
USTRUCT()
struct ANIMGRAPHRUNTIME_API FAnimNode_LayeredBoneBlend : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Links)
	FPoseLink BasePose;

	//@TODO: Anim: Comment these members
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category=Links)
	TArray<FPoseLink> BlendPoses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category=Config)
	TArray<FInputBlendPose> LayerSetup;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category=Runtime, meta=(PinShownByDefault))
	TArray<float> BlendWeights;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Config)
	bool bMeshSpaceRotationBlend;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Config)
	TEnumAsByte<enum ECurveBlendOption::Type>	CurveBlendOption;

	UPROPERTY(EditAnywhere, Category = Config)
	bool bBlendRootMotionBasedOnRootBone;

	UPROPERTY(Transient)
	bool bHasRelevantPoses;

protected:

	// This is buffer to serialize blend weight data for each joints
	// This has to save with the corresponding SkeletopnGuid
	// If not, it will rebuild in run-time
	UPROPERTY()
	TArray<FPerBoneBlendWeight>	PerBoneBlendWeights;

	UPROPERTY()
	FGuid						SkeletonGuid;

	UPROPERTY()
	FGuid						VirtualBoneGuid;


	// transient data to handle weight and target weight
	// this array changes based on required bones
	TArray<FPerBoneBlendWeight> DesiredBoneBlendWeights;
	TArray<FPerBoneBlendWeight> CurrentBoneBlendWeights;
	TArray<uint8> CurvePoseSourceIndices;

public:	
	FAnimNode_LayeredBoneBlend()
	{
		bBlendRootMotionBasedOnRootBone = true;
	}

	// FAnimNode_Base interface
	virtual void Initialize(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones(const FAnimationCacheBonesContext& Context) override;
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	void AddPose()
	{
		BlendWeights.Add(1.f);
		new (BlendPoses) FPoseLink();
		new (LayerSetup) FInputBlendPose();
	}

	void RemovePose(int32 PoseIndex)
	{
		BlendWeights.RemoveAt(PoseIndex);
		BlendPoses.RemoveAt(PoseIndex);
		LayerSetup.RemoveAt(PoseIndex);
	}

#if WITH_EDITOR
	// ideally you don't like to get to situation where it becomes inconsistent, but this happened, 
	// and we don't know what caused this. Possibly copy/paste, but I tried copy/paste and that didn't work
	// so here we add code to fix this up manually in editor, so that they can continue working on it. 
	void ValidateData();
	// FAnimNode_Base interface
	virtual void PostCompile(const class USkeleton* InSkeleton) override;
	// end FAnimNode_Base interface
#endif

	/** Reinitialize bone weights */
	void ReinitializeBoneBlendWeights(const FBoneContainer& RequiredBones, const USkeleton* Skeleton);

private:
	// Rebuild cache data from the skeleton
	void RebuildCacheData(const USkeleton* InSkeleton);
	bool IsCacheInvalid(const USkeleton* InSkeleton) const;
};
