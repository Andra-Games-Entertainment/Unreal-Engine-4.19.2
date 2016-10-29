// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "IntegerKeyArea.h"
#include "SIntegralCurveKeyEditor.h"


/* IKeyArea interface
 *****************************************************************************/

bool FIntegerKeyArea::CanCreateKeyEditor()
{
	return true;
}


TSharedRef<SWidget> FIntegerKeyArea::CreateKeyEditor(ISequencer* Sequencer)
{
	return SNew(SIntegralCurveKeyEditor<int32>)
		.Sequencer(Sequencer)
		.OwningSection(OwningSection)
		.Curve(&Curve)
		.ExternalValue(ExternalValue);
};
