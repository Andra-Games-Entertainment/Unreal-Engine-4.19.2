// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Models/DeviceManagerModel.h"

class FUICommandList;

/**
 * Implements the device toolbar widget.
 */
class SDeviceToolbar
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDeviceToolbar) { }
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 * @param InModel The view model to use.
	 * @param InUICommandList The UI command list to use.
	 */
	void Construct( const FArguments& InArgs, const FDeviceManagerModelRef& InModel, const TSharedPtr<FUICommandList>& InUICommandList );

private:

	// Callback for getting the enabled state of the toolbar.
	bool HandleToolbarIsEnabled( ) const;

private:

	// Holds a pointer the device manager's view model.
	FDeviceManagerModelPtr Model;
};
