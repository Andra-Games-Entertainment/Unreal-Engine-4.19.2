// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "IPropertyUtilities.h"
#include "IDetailTreeNode.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/STableRow.h"
#include "DetailCategoryBuilderImpl.h"
#include "SDetailTableRowBase.h"

class SDetailCategoryTableRow : public SDetailTableRowBase
{
public:
	SLATE_BEGIN_ARGS( SDetailCategoryTableRow )
		: _InnerCategory( false )
		, _ShowBorder( true )
		, _ColumnSizeData(nullptr)
	{}
		SLATE_ARGUMENT( FText, DisplayName )
		SLATE_ARGUMENT( bool, InnerCategory )
		SLATE_ARGUMENT( TSharedPtr<SWidget>, HeaderContent )
		SLATE_ARGUMENT( bool, ShowBorder )
		SLATE_ARGUMENT( const FDetailColumnSizeData*, ColumnSizeData )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedRef<IDetailTreeNode> InOwnerTreeNode, const TSharedRef<STableViewBase>& InOwnerTableView );
private:
	EVisibility IsSeparatorVisible() const;
	const FSlateBrush* GetBackgroundImage() const;
private:
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
	void OnColumnResized(float InNewWidth);
private:
	bool bIsInnerCategory;
	bool bShowBorder;
};


class FDetailCategoryGroupNode : public IDetailTreeNode, public TSharedFromThis<FDetailCategoryGroupNode>
{
public:
	FDetailCategoryGroupNode( const FDetailNodeList& InChildNodes, FName InGroupName, FDetailCategoryImpl& InParentCategory );

public:

	void SetShowBorder(bool bInShowBorder) { bShowBorder = bInShowBorder; }
	bool GetShowBorder() const { return bShowBorder; }

	void SetHasSplitter(bool bInHasSplitter) { bHasSplitter = bInHasSplitter; }
	bool GetHasSplitter() const { return bHasSplitter; }

private:
	virtual IDetailsViewPrivate& GetDetailsView() const override{ return ParentCategory.GetDetailsView(); }
	virtual void OnItemExpansionChanged( bool bIsExpanded ) override {}
	virtual bool ShouldBeExpanded() const override { return true; }
	virtual ENodeVisibility GetVisibility() const override { return bShouldBeVisible ? ENodeVisibility::Visible : ENodeVisibility::HiddenDueToFiltering; }
	virtual TSharedRef< ITableRow > GenerateNodeWidget( const TSharedRef<STableViewBase>& OwnerTable, const FDetailColumnSizeData& ColumnSizeData, const TSharedRef<IPropertyUtilities>& PropertyUtilities, bool bAllowFavoriteSystem) override;
	virtual void GetChildren( TArray< TSharedRef<IDetailTreeNode> >& OutChildren )  override;
	virtual void FilterNode( const FDetailFilter& InFilter ) override;
	virtual void Tick( float DeltaTime ) override {}
	virtual bool ShouldShowOnlyChildren() const override { return false; }
	virtual FName GetNodeName() const override { return NAME_None; }
private:
	FDetailNodeList ChildNodes;
	FDetailCategoryImpl& ParentCategory;
	FName GroupName;
	bool bShouldBeVisible;

	bool bShowBorder;
	bool bHasSplitter;
};
