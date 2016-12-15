// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UserInterface/PropertyEditor/SPropertyEditorEditInline.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Images/SImage.h"
#include "PropertyEditorHelpers.h"
#include "ObjectPropertyNode.h"
#include "PropertyHandleImpl.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "Styling/SlateIconFinder.h"
#include "ConstructorHelpers.h"
#include "Editor.h"

class FPropertyEditorInlineClassFilter : public IClassViewerFilter
{
public:
	/** The Object Property, classes are examined for a child-of relationship of the property's class. */
	UObjectPropertyBase* ObjProperty;

	/** The Interface Property, classes are examined for implementing the property's class. */
	UInterfaceProperty* IntProperty;

	/** Whether or not abstract classes are allowed. */
	bool bAllowAbstract;

	/** Limits the visible classes to meeting IsA relationships with all the objects. */
	TSet< const UObject* > LimitToIsAOfAllObjects;

	bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs ) override
	{
		const bool bChildOfObjectClass = ObjProperty && InClass->IsChildOf(ObjProperty->PropertyClass);
		const bool bDerivedInterfaceClass = IntProperty && InClass->ImplementsInterface(IntProperty->InterfaceClass);

		bool bMatchesFlags = InClass->HasAnyClassFlags(CLASS_EditInlineNew) && 
			!InClass->HasAnyClassFlags(CLASS_Hidden|CLASS_HideDropDown|CLASS_Deprecated) &&
			(bAllowAbstract || !InClass->HasAnyClassFlags(CLASS_Abstract));

		if( (bChildOfObjectClass || bDerivedInterfaceClass) && bMatchesFlags )
		{
			// The class must satisfy a Is-A relationship with all the objects in this set.
			return InFilterFuncs->IfMatchesAll_ObjectsSetIsAClass(LimitToIsAOfAllObjects, InClass->ClassWithin) != EFilterReturn::Failed;
		}

		return false;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		const bool bChildOfObjectClass = InUnloadedClassData->IsChildOf(ObjProperty->PropertyClass);

		bool bMatchesFlags = InUnloadedClassData->HasAnyClassFlags(CLASS_EditInlineNew) && 
			!InUnloadedClassData->HasAnyClassFlags(CLASS_Hidden|CLASS_HideDropDown|CLASS_Deprecated) && 
			(bAllowAbstract || !InUnloadedClassData->HasAnyClassFlags((CLASS_Abstract)));

		if(bChildOfObjectClass && bMatchesFlags)
		{
			// The class must satisfy a Is-A relationship with all the objects in this set.
			return InFilterFuncs->IfMatchesAll_ObjectsSetIsAClass(LimitToIsAOfAllObjects, InUnloadedClassData) != EFilterReturn::Failed;
		}

		return false;
	}
};

void SPropertyEditorEditInline::Construct( const FArguments& InArgs, const TSharedRef< class FPropertyEditor >& InPropertyEditor )
{
	PropertyEditor = InPropertyEditor;

	ChildSlot
	[
		SAssignNew(ComboButton, SComboButton)
		.OnGetMenuContent(this, &SPropertyEditorEditInline::GenerateClassPicker)
		.ContentPadding(0)
		.ToolTipText(InPropertyEditor, &FPropertyEditor::GetValueAsText )
		.ButtonContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew( SImage )
				.Image( this, &SPropertyEditorEditInline::GetDisplayValueIcon )
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew( STextBlock )
				.Text( this, &SPropertyEditorEditInline::GetDisplayValueAsString )
				.Font( InArgs._Font )
			]
		]
	];
}

FText SPropertyEditorEditInline::GetDisplayValueAsString() const
{
	UObject* CurrentValue = NULL;
	FPropertyAccess::Result Result = PropertyEditor->GetPropertyHandle()->GetValue( CurrentValue );
	if( Result == FPropertyAccess::Success && CurrentValue != NULL )
	{
		return CurrentValue->GetClass()->GetDisplayNameText();
	}
	else
	{
		return PropertyEditor->GetValueAsText();
	}
}

const FSlateBrush* SPropertyEditorEditInline::GetDisplayValueIcon() const
{
	UObject* CurrentValue = nullptr;
	FPropertyAccess::Result Result = PropertyEditor->GetPropertyHandle()->GetValue( CurrentValue );
	if( Result == FPropertyAccess::Success && CurrentValue != nullptr )
	{
		return FSlateIconFinder::FindIconBrushForClass(CurrentValue->GetClass());
	}

	return nullptr;
}

void SPropertyEditorEditInline::GetDesiredWidth( float& OutMinDesiredWidth, float& OutMaxDesiredWidth )
{
	OutMinDesiredWidth = 250.0f;
	OutMaxDesiredWidth = 600.0f;
}

bool SPropertyEditorEditInline::Supports( const FPropertyNode* InTreeNode, int32 InArrayIdx )
{
	return InTreeNode
		&& InTreeNode->HasNodeFlags(EPropertyNodeFlags::EditInlineNew)
		&& InTreeNode->FindObjectItemParent()
		&& !InTreeNode->IsEditConst();
}

bool SPropertyEditorEditInline::Supports( const TSharedRef< class FPropertyEditor >& InPropertyEditor )
{
	const TSharedRef< FPropertyNode > PropertyNode = InPropertyEditor->GetPropertyNode();
	return SPropertyEditorEditInline::Supports( &PropertyNode.Get(), PropertyNode->GetArrayIndex() );
}

bool SPropertyEditorEditInline::IsClassAllowed( UClass* CheckClass, bool bAllowAbstract ) const
{
	check(CheckClass);
	return PropertyEditorHelpers::IsEditInlineClassAllowed( CheckClass, bAllowAbstract ) &&  CheckClass->HasAnyClassFlags(CLASS_EditInlineNew);
}

TSharedRef<SWidget> SPropertyEditorEditInline::GenerateClassPicker()
{
	FClassViewerInitializationOptions Options;
	Options.bShowUnloadedBlueprints = true;
	Options.bShowDisplayNames = true;

	TSharedPtr<FPropertyEditorInlineClassFilter> ClassFilter = MakeShareable( new FPropertyEditorInlineClassFilter );
	Options.ClassFilter = ClassFilter;
	ClassFilter->bAllowAbstract = false;

	const TSharedRef< FPropertyNode > PropertyNode = PropertyEditor->GetPropertyNode();
	UProperty* Property = PropertyNode->GetProperty();
	ClassFilter->ObjProperty = Cast<UObjectPropertyBase>( Property );
	ClassFilter->IntProperty = Cast<UInterfaceProperty>( Property );
	Options.bShowNoneOption = !(Property->PropertyFlags & CPF_NoClear);

	FObjectPropertyNode* ObjectPropertyNode = PropertyNode->FindObjectItemParent();
	if( ObjectPropertyNode )
	{
		for ( TPropObjectIterator Itor( ObjectPropertyNode->ObjectIterator() ); Itor; ++Itor )
		{
			UObject* OwnerObject = Itor->Get();
			ClassFilter->LimitToIsAOfAllObjects.Add( OwnerObject );
		}
	}

	Options.PropertyHandle = PropertyEditor->GetPropertyHandle();

	FOnClassPicked OnPicked( FOnClassPicked::CreateRaw( this, &SPropertyEditorEditInline::OnClassPicked ) );

	return FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, OnPicked);
}

void SPropertyEditorEditInline::OnClassPicked(UClass* InClass)
{
	TArray<FObjectBaseAddress> ObjectsToModify;
	TArray<FString> NewValues;

	const TSharedRef< FPropertyNode > PropertyNode = PropertyEditor->GetPropertyNode();
	FObjectPropertyNode* ObjectNode = PropertyNode->FindObjectItemParent();

	if( ObjectNode )
	{
		GEditor->BeginTransaction(TEXT("PropertyEditor"), NSLOCTEXT("PropertyEditor", "OnClassPicked", "Set Class"), PropertyNode->GetProperty());

		for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
		{
			FString NewValue;
			if (InClass)
			{
				UObject*		Object = Itor->Get();
				UObject*		UseOuter = (InClass->IsChildOf(UClass::StaticClass()) ? Cast<UClass>(Object)->GetDefaultObject() : Object);
				EObjectFlags	MaskedOuterFlags = UseOuter ? UseOuter->GetMaskedFlags(RF_PropagateToSubObjects) : RF_NoFlags;
				if (UseOuter && UseOuter->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
				{
					MaskedOuterFlags |= RF_ArchetypeObject;
				}
				UObject*		NewUObject = NewObject<UObject>(UseOuter, InClass, NAME_None, MaskedOuterFlags, NULL);

				NewValue = NewUObject->GetPathName();
			}
			else
			{
				NewValue = FName(NAME_None).ToString();
			}
			NewValues.Add(NewValue);
		}

		const TSharedRef< IPropertyHandle > PropertyHandle = PropertyEditor->GetPropertyHandle();

		// If this is an instanced component property collect current component names so we can clean them properly if necessary
		TArray<FString> PrevPerObjectValues;
		UObjectProperty* ObjectProperty = CastChecked<UObjectProperty>(PropertyHandle->GetProperty());
		if (ObjectProperty && ObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference) && ObjectProperty->PropertyClass->IsChildOf(UActorComponent::StaticClass()))
		{
			PropertyHandle->GetPerObjectValues(PrevPerObjectValues);
		}

		PropertyHandle->SetPerObjectValues(NewValues);
		check(PrevPerObjectValues.Num() == 0 || PrevPerObjectValues.Num() == NewValues.Num());

		for (int32 Index = 0; Index < PrevPerObjectValues.Num(); ++Index)
		{
			if (PrevPerObjectValues[Index] != NewValues[Index])
			{
				// Move the old component to the transient package so resetting owned components on the parent doesn't find it
				ConstructorHelpers::StripObjectClass(PrevPerObjectValues[Index]);
				if (UActorComponent* Component = Cast<UActorComponent>(StaticFindObject(UActorComponent::StaticClass(), ANY_PACKAGE, *PrevPerObjectValues[Index])))
				{
					Component->Modify();
					Component->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
				}
			}
		}

		// End the transaction if we called PreChange
		GEditor->EndTransaction();

		// Force a rebuild of the children when this node changes
		PropertyNode->RequestRebuildChildren();

		ComboButton->SetIsOpen(false);
	}
}
