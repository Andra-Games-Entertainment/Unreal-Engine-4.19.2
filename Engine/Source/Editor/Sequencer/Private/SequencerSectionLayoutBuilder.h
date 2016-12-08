// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "IKeyArea.h"
#include "DisplayNodes/SequencerTrackNode.h"
#include "ISectionLayoutBuilder.h"

class FSequencerSectionLayoutBuilder
	: public ISectionLayoutBuilder
{
public:
	FSequencerSectionLayoutBuilder( TSharedRef<FSequencerTrackNode> InRootNode );

public:

	// ISectionLayoutBuilder interface

	virtual void PushCategory( FName CategoryName, const FText& DisplayLabel ) override;
	virtual void SetSectionAsKeyArea( TSharedRef<IKeyArea> KeyArea ) override;
	virtual void AddKeyArea( FName KeyAreaName, const FText& DisplayName, TSharedRef<IKeyArea> KeyArea ) override;
	virtual void PopCategory() override;

private:

	/** Root node of the tree */
	TSharedRef<FSequencerTrackNode> RootNode;

	/** The current node that other nodes are added to */
	TSharedRef<FSequencerDisplayNode> CurrentNode;
};
