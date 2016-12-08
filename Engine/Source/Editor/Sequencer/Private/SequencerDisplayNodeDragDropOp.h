// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/DragAndDrop.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "DragAndDrop/DecoratedDragDropOp.h"

/** A decorated drag drop operation object for dragging sequencer display nodes. */
class FSequencerDisplayNodeDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE( FSequencerDisplayNodeDragDropOp, FDecoratedDragDropOp )

public:
	FSequencerDisplayNodeDragDropOp( TArray<TSharedRef<FSequencerDisplayNode>>& InDraggedNodes );

	/** Gets the nodes which are currently being dragged. */
	TArray<TSharedRef<FSequencerDisplayNode>>& GetDraggedNodes();

private:
	/** The nodes currently being dragged. */
	TArray<TSharedRef<FSequencerDisplayNode>> DraggedNodes;
};
