// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalControl.cpp: SkeletalControl code and related.
=============================================================================*/ 

#include "CoreMinimal.h"
#include "BoneContainer.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/BoneMaskFilter.h"
#include "EngineLogs.h"

DEFINE_LOG_CATEGORY(LogSkeletalControl);

/////////////////////////////////////////////////////
// UBoneMaskFilter 

UBoneMaskFilter::UBoneMaskFilter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/////////////////////////////////////////////////////
// FBoneReference

bool FBoneReference::Initialize(const FBoneContainer& RequiredBones)
{
	BoneName = *BoneName.ToString().Trim().TrimTrailing();
	BoneIndex = RequiredBones.GetPoseBoneIndexForBoneName(BoneName);

	bUseSkeletonIndex = false;
	// If bone name is not found, look into the master skeleton to see if it's found there.
	// SkeletalMeshes can exclude bones from the master skeleton, and that's OK.
	// If it's not found in the master skeleton, the bone does not exist at all! so we should report it as a warning.
	if (BoneIndex == INDEX_NONE && BoneName != NAME_None)
	{
		if (USkeleton* SkeletonAsset = RequiredBones.GetSkeletonAsset())
		{
			if (SkeletonAsset->GetReferenceSkeleton().FindBoneIndex(BoneName) == INDEX_NONE)
			{
				UE_LOG(LogAnimation, Warning, TEXT("FBoneReference::Initialize BoneIndex for Bone '%s' does not exist in Skeleton '%s'"), 
					*BoneName.ToString(), *GetNameSafe(SkeletonAsset));
			}
		}
	}

	return (BoneIndex != INDEX_NONE);
}

bool FBoneReference::Initialize(const USkeleton* Skeleton)
{
	if( Skeleton && (BoneName != NAME_None) )
	{
		BoneName = *BoneName.ToString().Trim().TrimTrailing();
		BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);
		bUseSkeletonIndex = true;
	}
	else
	{
		BoneIndex = INDEX_NONE;
	}

	return (BoneIndex != INDEX_NONE);
}

bool FBoneReference::IsValid(const FBoneContainer& RequiredBones) const
{
	return (BoneIndex != INDEX_NONE && RequiredBones.Contains(BoneIndex));
}

