// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyEditing.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyRestriction.h"
#include "SMultipleOptionTable.h"

#define LOCTEXT_NAMESPACE "FProjectPackagingSettingsCustomization"

class SCulturePickerRowWidget : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SCulturePickerRowWidget){}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, FCulturePtr InCulture, TAttribute<bool> InIsFilteringCultures)
	{
		Culture = InCulture;
		IsFilteringCultures = InIsFilteringCultures;

		// Identify if this culture has localization data.
		TArray< FCultureRef > LocalizedCultures;
		TArray<FString> LocalizationPaths = FPaths::GetGameLocalizationPaths();
		FInternationalization::Get().GetCulturesWithAvailableLocalization(LocalizationPaths, LocalizedCultures, true);
		HasLocalizationData = LocalizedCultures.Contains(Culture.ToSharedRef());

		ChildSlot
			[
				SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(3.0, 2.0))
					.VAlign(VAlign_Center)
					[
						// Warning Icon for whether or not this culture has localization data.
						SNew(SImage)
						.Image( FCoreStyle::Get().GetBrush("Icons.Warning") )
						.Visibility(this, &SCulturePickerRowWidget::HandleWarningImageVisibility)
						.ToolTipText(LOCTEXT("NotLocalizedWarning", "This project does not have localization data (translations) for this culture."))
					]
					+SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						// Display name of culture.
						SNew(STextBlock)
						.Text(FText::FromString(Culture->GetDisplayName()))
						.ToolTipText(FText::FromString(Culture->GetName()))
					]
			];
	}

	EVisibility HandleWarningImageVisibility() const
	{
		// Don't show the warning image if this culture has localization data.
		// Collapse the widget entirely if we are filtering to only show cultures that have it - gets rid of awkward empty column of space.
		bool bIsFilteringCultures = IsFilteringCultures.IsBound() ? IsFilteringCultures.Get() : false;
		return bIsFilteringCultures ? EVisibility::Collapsed : (HasLocalizationData ? EVisibility::Hidden : EVisibility::Visible);
	}

private:
	FCulturePtr Culture;
	TAttribute<bool> IsFilteringCultures;
	bool HasLocalizationData;
};

/**
 * Implements a details view customization for UProjectPackagingSettingsCustomization objects.
 */
class FProjectPackagingSettingsCustomization
	: public IDetailCustomization
{
public:

	// IDetailCustomization interface
	virtual void CustomizeDetails( IDetailLayoutBuilder& LayoutBuilder ) override
	{
		CustomizeProjectCategory(LayoutBuilder);
		CustomizePackagingCategory(LayoutBuilder);
	}

public:

	/**
	 * Creates a new instance.
	 *
	 * @return A new struct customization for play-in settings.
	 */
	static TSharedRef<IDetailCustomization> MakeInstance( )
	{
		return MakeShareable(new FProjectPackagingSettingsCustomization());
	}

protected:

	enum class EFilterCulturesChoices
	{
		/**
		 * Only show cultures that have localization data.
		 */
		OnlyLocalizedCultures,

		/**
		 * Show all available cultures.
		 */
		AllAvailableCultures
	};

	FProjectPackagingSettingsCustomization()
		: FilterCulturesChoice(EFilterCulturesChoices::AllAvailableCultures)
		, IsInBatchSelectOperation(false)
	{

	}

	/**
	 * Customizes the Project property category.
	 *
	 * @param LayoutBuilder The layout builder.
	 */
	void CustomizeProjectCategory( IDetailLayoutBuilder& LayoutBuilder )
	{
		// hide the DebugGame configuration for content-only games
		TArray<FString> TargetFileNames;
		IFileManager::Get().FindFiles(TargetFileNames, *(FPaths::GameSourceDir() / TEXT("*.target.cs")), true, false);

		if (TargetFileNames.Num() == 0)
		{
			IDetailCategoryBuilder& ProjectCategory = LayoutBuilder.EditCategory("Project");
			{
				TSharedRef<FPropertyRestriction> BuildConfigurationRestriction = MakeShareable(new FPropertyRestriction(LOCTEXT("DebugGameRestrictionReason", "The DebugGame build configuration is not available in content-only projects.")));
				BuildConfigurationRestriction->AddValue("DebugGame");

				TSharedRef<IPropertyHandle> BuildConfigurationHandle = LayoutBuilder.GetProperty("BuildConfiguration");
				BuildConfigurationHandle->AddRestriction(BuildConfigurationRestriction);
			}
		}
	}

	/**
	 * Customizes the Packaging property category.
	 *
	 * @param LayoutBuilder The layout builder.
	 */
	void CustomizePackagingCategory( IDetailLayoutBuilder& LayoutBuilder )
	{
		IDetailCategoryBuilder& PackagingCategory = LayoutBuilder.EditCategory("Packaging");
		{
			CulturesPropertyHandle = LayoutBuilder.GetProperty("CulturesToStage", UProjectPackagingSettings::StaticClass());
			CulturesPropertyHandle->MarkHiddenByCustomization();
			CulturesPropertyArrayHandle = CulturesPropertyHandle->AsArray();

			PopulateCultureList();

			PackagingCategory.AddCustomRow(LOCTEXT("CulturesToStageLabel", "Cultures To Stage"), true)
				.NameContent()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Top)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						CulturesPropertyHandle->CreatePropertyNameWidget()
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush(TEXT("Icons.Error")))
						.ToolTipText(LOCTEXT("NoCulturesToStageSelectedError", "At least one culture must be selected or fatal errors may occur when launching games."))
						.Visibility(this, &FProjectPackagingSettingsCustomization::HandleNoCulturesErrorIconVisibility)
					]
				]
				.ValueContent()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							// all cultures radio button
							SNew(SCheckBox)
							.IsChecked(this, &FProjectPackagingSettingsCustomization::HandleShowCulturesCheckBoxIsChecked, EFilterCulturesChoices::AllAvailableCultures)
							.OnCheckStateChanged(this, &FProjectPackagingSettingsCustomization::HandleShowCulturesCheckBoxCheckStateChanged, EFilterCulturesChoices::AllAvailableCultures)
							.Style(FEditorStyle::Get(), "RadioButton")
							[
								SNew(STextBlock)
								.Text(LOCTEXT("AllCulturesCheckBoxText", "Show all"))
							]
						]

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							// localized cultures radio button
							SNew(SCheckBox)
							.IsChecked(this, &FProjectPackagingSettingsCustomization::HandleShowCulturesCheckBoxIsChecked, EFilterCulturesChoices::OnlyLocalizedCultures)
							.OnCheckStateChanged(this, &FProjectPackagingSettingsCustomization::HandleShowCulturesCheckBoxCheckStateChanged, EFilterCulturesChoices::OnlyLocalizedCultures)
							.Style(FEditorStyle::Get(), "RadioButton")
							[
								SNew(STextBlock)
								.Text(LOCTEXT("CookedCulturesCheckBoxText", "Show localized"))
							]
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(Table, SMultipleOptionTable<FCulturePtr>, &CultureList)
						.OnPreBatchSelect(this, &FProjectPackagingSettingsCustomization::OnPreBatchSelect)
						.OnPostBatchSelect(this, &FProjectPackagingSettingsCustomization::OnPostBatchSelect)
						.OnGenerateOptionWidget(this, &FProjectPackagingSettingsCustomization::GenerateWidgetForCulture)
						.OnOptionSelectionChanged(this, &FProjectPackagingSettingsCustomization::OnCultureSelectionChanged)
						.IsOptionSelected(this, &FProjectPackagingSettingsCustomization::IsCultureSelected)
						.ListHeight(100.0f)
					]
				];
		}
	}

	void PopulateCultureList()
	{
		switch(FilterCulturesChoice)
		{
		case EFilterCulturesChoices::AllAvailableCultures:
			{
				CultureList.Empty();
				TArray<FString> CultureNames;
				FInternationalization::Get().GetCultureNames(CultureNames);
				for(const FString& CultureName : CultureNames)
				{
					CultureList.Add(FInternationalization::Get().GetCulture(CultureName));
				}
			}
			break;
		case EFilterCulturesChoices::OnlyLocalizedCultures:
			{
				TArray<FCultureRef> LocalizedCultureList;
				TArray<FString> LocalizationPaths = FPaths::GetGameLocalizationPaths();
				FInternationalization::Get().GetCulturesWithAvailableLocalization(LocalizationPaths, LocalizedCultureList, true);
				CultureList.Empty();
				for(const auto& Culture : LocalizedCultureList)
				{
					CultureList.Add(Culture);
				}
			}
			break;
		}
	}

	EVisibility HandleNoCulturesErrorIconVisibility() const
	{
		TArray<void*> RawData;
		CulturesPropertyHandle->AccessRawData(RawData);
		TArray<FString>* RawCultureStringArray = reinterpret_cast<TArray<FString>*>(RawData[0]);

		return RawCultureStringArray->Num() ? EVisibility::Hidden : EVisibility::Visible;
	}

	ESlateCheckBoxState::Type HandleShowCulturesCheckBoxIsChecked( EFilterCulturesChoices Choice ) const
	{
		if (FilterCulturesChoice == Choice)
		{
			return ESlateCheckBoxState::Checked;
		}

		return ESlateCheckBoxState::Unchecked;
	}

	void HandleShowCulturesCheckBoxCheckStateChanged( ESlateCheckBoxState::Type NewState, EFilterCulturesChoices Choice )
	{
		if (NewState == ESlateCheckBoxState::Checked)
		{
			FilterCulturesChoice = Choice;
		}

		PopulateCultureList();
		Table->RequestTableRefresh();
	}

	void AddCulture(FString CultureName)
	{
		if(!IsInBatchSelectOperation)
		{
			CulturesPropertyHandle->NotifyPreChange();
		}
		TArray<void*> RawData;
		CulturesPropertyHandle->AccessRawData(RawData);
		TArray<FString>* RawCultureStringArray = reinterpret_cast<TArray<FString>*>(RawData[0]);
		RawCultureStringArray->Add(CultureName);
		if(!IsInBatchSelectOperation)
		{
			CulturesPropertyHandle->NotifyPostChange();
		}
	}

	void RemoveCulture(FString CultureName)
	{
		if(!IsInBatchSelectOperation)
		{
			CulturesPropertyHandle->NotifyPreChange();
		}
		TArray<void*> RawData;
		CulturesPropertyHandle->AccessRawData(RawData);
		TArray<FString>* RawCultureStringArray = reinterpret_cast<TArray<FString>*>(RawData[0]);
		RawCultureStringArray->Remove(CultureName);
		if(!IsInBatchSelectOperation)
		{
			CulturesPropertyHandle->NotifyPostChange();
		}
	}

	bool IsFilteringCultures() const
	{
		return FilterCulturesChoice == EFilterCulturesChoices::OnlyLocalizedCultures;
	}

	void OnPreBatchSelect()
	{
		IsInBatchSelectOperation = true;
		CulturesPropertyHandle->NotifyPreChange();
	}

	void OnPostBatchSelect()
	{
		CulturesPropertyHandle->NotifyPostChange();
		IsInBatchSelectOperation = false;
	}

	TSharedRef<SWidget> GenerateWidgetForCulture(FCulturePtr Culture)
	{
		return SNew(SCulturePickerRowWidget, Culture, TAttribute<bool>(this, &FProjectPackagingSettingsCustomization::IsFilteringCultures));
	}

	void OnCultureSelectionChanged(bool IsSelected, FCulturePtr Culture)
	{
		if(IsSelected)
		{
			AddCulture(Culture->GetName());
		}
		else
		{
			RemoveCulture(Culture->GetName());
		}
		
	}

	bool IsCultureSelected(FCulturePtr Culture)
	{
		FString CultureName = Culture->GetName();

		uint32 ElementCount;
		CulturesPropertyArrayHandle->GetNumElements(ElementCount);
		for(uint32 Index = 0; Index < ElementCount; ++Index)
		{
			const TSharedRef<IPropertyHandle> PropertyHandle = CulturesPropertyArrayHandle->GetElement(Index);
			FString CultureNameAtIndex;
			PropertyHandle->GetValue(CultureNameAtIndex);
			if(CultureNameAtIndex == CultureName)
			{
				return true;
			}
		}

		return false;
	}

private:
	TArray<FCulturePtr> CultureList;
	TSharedPtr<IPropertyHandle> CulturesPropertyHandle;
	TSharedPtr<IPropertyHandleArray> CulturesPropertyArrayHandle;
	EFilterCulturesChoices FilterCulturesChoice;
	TSharedPtr< SMultipleOptionTable<FCulturePtr> > Table;
	bool IsInBatchSelectOperation;
};


#undef LOCTEXT_NAMESPACE
