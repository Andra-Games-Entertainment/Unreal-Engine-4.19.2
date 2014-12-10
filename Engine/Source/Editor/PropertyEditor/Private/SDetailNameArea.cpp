// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "PropertyEditorPrivatePCH.h"
#include "SDetailNameArea.h"
#include "ClassIconFinder.h"
#include "Editor/EditorWidgets/Public/EditorWidgets.h"
#include "EditorClassUtils.h"

#define LOCTEXT_NAMESPACE "SDetailsView"

void SDetailNameArea::Construct( const FArguments& InArgs, const TArray< TWeakObjectPtr<UObject> >* SelectedObjects )
{
	OnLockButtonClicked = InArgs._OnLockButtonClicked;
	IsLocked = InArgs._IsLocked;
	SelectionTip = InArgs._SelectionTip;
	bShowLockButton = InArgs._ShowLockButton;
	bShowActorLabel = InArgs._ShowActorLabel;
	Refresh( *SelectedObjects );
}

void SDetailNameArea::Refresh( const TArray< TWeakObjectPtr<UObject> >& SelectedObjects )
{
	ChildSlot
	[
		BuildObjectNameArea( SelectedObjects )
	];
}

void SDetailNameArea::Refresh( const TArray< TWeakObjectPtr<AActor> >& SelectedActors )
{
	// Convert the actor array to base object type
	TArray< TWeakObjectPtr<UObject> > SelectedObjects;
	for( int32 ActorIndex = 0 ; ActorIndex < SelectedActors.Num() ; ++ActorIndex )
	{
		const TWeakObjectPtr<UObject> ObjectWeakPtr = SelectedActors[ActorIndex].Get();
		SelectedObjects.Add( ObjectWeakPtr );
	}

	Refresh( SelectedObjects );
}

const FSlateBrush* SDetailNameArea::OnGetLockButtonImageResource() const
{
	if( IsLocked.Get() )
	{
		return FEditorStyle::GetBrush(TEXT("PropertyWindow.Locked"));
	}
	else
	{
		return FEditorStyle::GetBrush(TEXT("PropertyWindow.Unlocked"));
	}
}

TSharedRef< SWidget > SDetailNameArea::BuildObjectNameArea( const TArray< TWeakObjectPtr<UObject> >& SelectedObjects )
{
	// Get the common base class of the selected objects
	UClass* BaseClass = NULL;
	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		TWeakObjectPtr<UObject> ObjectWeakPtr = SelectedObjects[ObjectIndex];
		if( ObjectWeakPtr.IsValid() )
		{
			UClass* ObjClass = ObjectWeakPtr->GetClass();

			if (!BaseClass)
			{
				BaseClass = ObjClass;
			}

			while (!ObjClass->IsChildOf(BaseClass))
			{
				BaseClass = BaseClass->GetSuperClass();
			}
		}
	}

	TSharedRef< SHorizontalBox > ObjectNameArea = SNew( SHorizontalBox );

	if (BaseClass)
	{
		// Get selection icon based on actor(s) classes and add before the selection label
		const FSlateBrush* ActorIcon = FClassIconFinder::FindIconForClass(BaseClass);

		ObjectNameArea->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(0,0,0,0)
		[
			SNew(SImage)
			.Image(ActorIcon)
			.ToolTip(FEditorClassUtils::GetTooltip(BaseClass))
		];
	}


	// Add the selected object(s) type name, along with buttons for either opening C++ code or editing blueprints
	const int32 NumSelectedSurfaces = AssetSelectionUtils::GetNumSelectedSurfaces( GWorld );
	if( SelectedObjects.Num() > 0 )
	{
		if ( bShowActorLabel )
		{
			FEditorWidgetsModule& EdWidgetsModule = FModuleManager::LoadModuleChecked<FEditorWidgetsModule>(TEXT("EditorWidgets"));
			TSharedRef<IObjectNameEditableTextBox> ObjectNameBox = EdWidgetsModule.CreateObjectNameEditableTextBox(SelectedObjects);

			ObjectNameArea->AddSlot()
				.AutoWidth()
				.Padding(0, 0, 6, 0)
				[
					SNew(SBox)
					.WidthOverride(200.0f)
					.VAlign(VAlign_Center)
					[
						ObjectNameBox
					]
				];
		}

		const TWeakObjectPtr< UObject > ObjectWeakPtr = SelectedObjects.Num() == 1 ? SelectedObjects[0] : NULL;
		BuildObjectNameAreaSelectionLabel( ObjectNameArea, ObjectWeakPtr, SelectedObjects.Num() );

		if( bShowLockButton )
		{
			ObjectNameArea->AddSlot()
				.HAlign(HAlign_Right)
				.FillWidth(1.0f)
				[
					SNew( SButton )
					.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
					.OnClicked(	OnLockButtonClicked )
					.ToolTipText( LOCTEXT("LockSelectionButton_ToolTip", "Locks the current selection into the Details panel") )
					[
						SNew( SImage )
						.Image( this, &SDetailNameArea::OnGetLockButtonImageResource )
					]
				];
		}
	}
	else
	{
		if ( SelectionTip.Get() && NumSelectedSurfaces == 0 )
		{
			ObjectNameArea->AddSlot()
			.FillWidth( 1.0f )
			.HAlign( HAlign_Center )
			.Padding( 2.0f, 24.0f, 2.0f, 2.0f )
			[
				SNew( STextBlock )
				.Text( LOCTEXT("NoObjectsSelected", "Select an object to view details.") )
			];
		}
		else
		{
			// Fill the empty space
			ObjectNameArea->AddSlot();
		}
	}
	
	return ObjectNameArea;
}

void SDetailNameArea::BuildObjectNameAreaSelectionLabel( TSharedRef< SHorizontalBox > SelectionLabelBox, const TWeakObjectPtr<UObject> ObjectWeakPtr, const int32 NumSelectedObjects ) 
{
	check( NumSelectedObjects > 1 || ObjectWeakPtr.IsValid() );

	if( NumSelectedObjects == 1 )
	{
		UClass* ObjectClass = ObjectWeakPtr.Get()->GetClass();
		if( ObjectClass != nullptr )
		{
			SelectionLabelBox->AddSlot()
				.AutoWidth()
				.VAlign( VAlign_Center )
				.HAlign( HAlign_Left )
				.Padding( 1.0f, 1.0f, 0.0f, 0.0f )
				[
					FEditorClassUtils::GetDocumentationLinkWidget(ObjectClass)
				];

			SelectionLabelBox->AddSlot()
				.AutoWidth()
				.VAlign( VAlign_Center )
				.HAlign( HAlign_Left )
				.Padding( 6.0f, 1.0f, 0.0f, 0.0f )
				[
					FEditorClassUtils::GetSourceLink(ObjectClass, ObjectWeakPtr)
				];
		}
	}
	else
	{
		const FString SelectionText = FString::Printf( *LOCTEXT("MultipleObjectsSelected", "%d objects").ToString(), NumSelectedObjects );
		SelectionLabelBox->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign( HAlign_Left )
		.FillWidth( 1.0f )
		[
			SNew(STextBlock)
			.Text( SelectionText )
		];

	}
}


#undef LOCTEXT_NAMESPACE

