// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Layout/WidgetCaching.h"
#include "Types/PaintArgs.h"
#include "Input/HittestGrid.h"

// FCachedWidgetNode
//-------------------------------------------------------------

void FCachedWidgetNode::Initialize(const FPaintArgs& Args, TSharedRef<SWidget> InWidget, const FGeometry& InGeometry, const FSlateRect& InClippingRect)
{
	Widget = InWidget;
	Geometry = InGeometry;
	ClippingRect = InClippingRect;
	WindowOffset = Args.GetWindowToDesktopTransform();
	RecordedVisibility = Args.GetLastRecordedVisibility();
	LastRecordedHittestIndex = Args.GetLastHitTestIndex();

	if ( RecordedVisibility.AreChildrenHitTestVisible() )
	{
		RecordedVisibility = InWidget->GetVisibility();
	}

	Children.Reset();
}

void FCachedWidgetNode::RecordHittestGeometry(FHittestGrid& Grid, int32 LastHittestIndex, int32 LayerId)
{
	if ( RecordedVisibility.AreChildrenHitTestVisible() )
	{
		TSharedPtr<SWidget> SafeWidget = Widget.Pin();
		if ( SafeWidget.IsValid() )
		{
			LastRecordedHittestIndex = LastHittestIndex;

			const int32 ChildCount = Children.Num();
			for ( int32 i = 0; i < ChildCount; i++ )
			{
				Children[i]->RecordHittestGeometryInternal(Grid, LastHittestIndex, LayerId);
			}
		}
	}
}

void FCachedWidgetNode::RecordHittestGeometryInternal(FHittestGrid& Grid, int32 LastHittestIndex, int32 LayerId)
{
	if ( RecordedVisibility.AreChildrenHitTestVisible() )
	{
		TSharedPtr<SWidget> SafeWidget = Widget.Pin();
		if ( SafeWidget.IsValid() )
		{
			LastRecordedHittestIndex = Grid.InsertWidget(LastHittestIndex, RecordedVisibility, FArrangedWidget(SafeWidget.ToSharedRef(), Geometry), WindowOffset, ClippingRect, LayerId);

			const int32 ChildCount = Children.Num();
			for ( int32 i = 0; i < ChildCount; i++ )
			{
				Children[i]->RecordHittestGeometryInternal(Grid, LastRecordedHittestIndex, LayerId);
			}
		}
	}
}
