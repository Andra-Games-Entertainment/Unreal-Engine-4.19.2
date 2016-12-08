// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetData.h"
#include "Input/DragAndDrop.h"
#include "Layout/Visibility.h"
#include "DragAndDrop/DecoratedDragDropOp.h"

class FAssetThumbnail;
class FAssetThumbnailPool;
class UActorFactory;

class FAssetDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FAssetDragDropOp, FDecoratedDragDropOp)

	/** Data for the asset this item represents */
	TArray<FAssetData> AssetData;

	/** Pool for maintaining and rendering thumbnails */
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;

	/** Handle to the thumbnail resource */
	TSharedPtr<FAssetThumbnail> AssetThumbnail;

	/** The actor factory to use if converting this asset to an actor */
	TWeakObjectPtr< UActorFactory > ActorFactory;

	UNREALED_API static TSharedRef<FAssetDragDropOp> New(const FAssetData& InAssetData, UActorFactory* ActorFactory = NULL);

	UNREALED_API static TSharedRef<FAssetDragDropOp> New(const TArray<FAssetData>& InAssetData, UActorFactory* ActorFactory = NULL);

public:
	UNREALED_API virtual ~FAssetDragDropOp();

	UNREALED_API virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;

	UNREALED_API void Init();

	UNREALED_API EVisibility GetTooltipVisibility() const;

private:
	int32 ThumbnailSize;
};
