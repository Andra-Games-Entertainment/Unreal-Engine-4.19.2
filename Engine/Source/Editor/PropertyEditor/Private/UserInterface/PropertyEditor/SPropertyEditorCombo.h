// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "EditorStyleSet.h"
#include "Presentation/PropertyEditor/PropertyEditor.h"
#include "UserInterface/PropertyEditor/PropertyEditorConstants.h"

class SPropertyComboBox;

class SPropertyEditorCombo : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SPropertyEditorCombo )
		: _Font( FEditorStyle::GetFontStyle( PropertyEditorConstants::PropertyFontStyle ) ) 
		{}
		SLATE_ARGUMENT( FSlateFontInfo, Font )
	SLATE_END_ARGS()

	static bool Supports( const TSharedRef< class FPropertyEditor >& InPropertyEditor );

	void Construct( const FArguments& InArgs, const TSharedRef< class FPropertyEditor >& InPropertyEditor );

	void GetDesiredWidth( float& OutMinDesiredWidth, float& OutMaxDesiredWidth );
private:
	void GenerateComboBoxStrings( TArray< TSharedPtr<FString> >& OutComboBoxStrings, TArray<TSharedPtr<class SToolTip>>& OutToolTips, TArray<bool>& OutRestrictedItems );
	void OnComboSelectionChanged( TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo );
	void OnComboOpening();

	virtual void SendToObjects( const FString& NewValue );

	/**
	 * Gets the active display value as a string
	 */
	FString GetDisplayValueAsString() const;

	/** @return True if the property can be edited */
	bool CanEdit() const;
private:

	TSharedPtr< class FPropertyEditor > PropertyEditor;

	/** Fills out with generated strings. */
	TSharedPtr<class SPropertyComboBox> ComboBox;

	/**
	 * Indicates that this combo box's values are friendly names for the real values; currently only used for enum drop-downs.
	 */
	bool bUsesAlternateDisplayValues;
};
