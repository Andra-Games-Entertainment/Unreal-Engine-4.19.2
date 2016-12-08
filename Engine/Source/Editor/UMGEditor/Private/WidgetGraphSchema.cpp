// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WidgetGraphSchema.h"

UWidgetGraphSchema::UWidgetGraphSchema(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	NAME_NeverAsPin = TEXT("NeverAsPin");
	NAME_PinHiddenByDefault = TEXT("PinHiddenByDefault");
	NAME_PinShownByDefault = TEXT("PinShownByDefault");
	NAME_AlwaysAsPin = TEXT("AlwaysAsPin");
	NAME_OnEvaluate = TEXT("OnEvaluate");
	DefaultEvaluationHandlerName = TEXT("EvaluateGraphExposedInputs");
}
