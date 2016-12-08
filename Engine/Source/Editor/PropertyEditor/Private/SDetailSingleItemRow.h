// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "IDetailTreeNode.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SDetailsViewBase.h"
#include "SDetailTableRowBase.h"

class IDetailKeyframeHandler;
struct FDetailLayoutCustomization;

/**
 * A widget for details that span the entire tree row and have no columns                                                              
 */
class SDetailSingleItemRow : public SDetailTableRowBase
{
public:
	SLATE_BEGIN_ARGS( SDetailSingleItemRow )
		: _ColumnSizeData() {}

		SLATE_ARGUMENT( FDetailColumnSizeData, ColumnSizeData )
		SLATE_ARGUMENT( bool, AllowFavoriteSystem)
	SLATE_END_ARGS()

	/**
	 * Construct the widget
	 */
	void Construct( const FArguments& InArgs, FDetailLayoutCustomization* InCustomization, bool bHasMultipleColumns, TSharedRef<IDetailTreeNode> InOwnerTreeNode, const TSharedRef<STableViewBase>& InOwnerTableView );
protected:
	virtual bool OnContextMenuOpening( FMenuBuilder& MenuBuilder ) override;
private:
	void OnLeftColumnResized( float InNewWidth );
	void OnCopyProperty();
	void OnPasteProperty();
	bool CanPasteProperty() const;
	const FSlateBrush* GetBorderImage() const;
	TSharedRef<SWidget> CreateExtensionWidget( TSharedRef<SWidget> ValueWidget, FDetailLayoutCustomization& InCustomization, TSharedRef<IDetailTreeNode> InTreeNode );
	TSharedRef<SWidget> CreateKeyframeButton( FDetailLayoutCustomization& InCustomization, TSharedRef<IDetailTreeNode> InTreeNode );
	bool IsKeyframeButtonEnabled(TSharedRef<IDetailTreeNode> InTreeNode) const;
	FReply OnAddKeyframeClicked();
	bool IsHighlighted() const;

	const FSlateBrush* GetFavoriteButtonBrush() const;
	FReply OnFavoriteToggle();
	void AllowShowFavorite();
private:
	TWeakPtr<IDetailKeyframeHandler> KeyframeHandler;
	/** Customization for this widget */
	FDetailLayoutCustomization* Customization;
	FDetailColumnSizeData ColumnSizeData;
	bool bAllowFavoriteSystem;
};
