// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MacTargetSettingsDetails.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Layout/Margin.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorDirectories.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "SExternalImageReference.h"
#include "SNumericDropDown.h"
#include "Dialogs/Dialogs.h"
#include "Widgets/Notifications/SErrorText.h"
#include "IDetailPropertyRow.h"

namespace MacTargetSettingsDetailsConstants
{
	/** The filename for the game splash screen */
	const FString GameSplashFileName(TEXT("Splash/Splash.bmp"));

	/** The filename for the editor splash screen */
	const FString EditorSplashFileName(TEXT("Splash/EdSplash.bmp"));
}

#define LOCTEXT_NAMESPACE "MacTargetSettingsDetails"

TSharedRef<IDetailCustomization> FMacTargetSettingsDetails::MakeInstance()
{
	return MakeShareable(new FMacTargetSettingsDetails);
}

namespace EMacImageScope
{
	enum Type
	{
		Engine,
		GameOverride
	};
}

/* Helper function used to generate filenames for splash screens */
static FString GetSplashFilename(EMacImageScope::Type Scope, bool bIsEditorSplash)
{
	FString Filename;

	if (Scope == EMacImageScope::Engine)
	{
		Filename = FPaths::EngineContentDir();
	}
	else
	{
		Filename = FPaths::GameContentDir();
	}

	if(bIsEditorSplash)
	{
		Filename /= MacTargetSettingsDetailsConstants::EditorSplashFileName;
	}
	else
	{
		Filename /= MacTargetSettingsDetailsConstants::GameSplashFileName;
	}

	Filename = FPaths::ConvertRelativePathToFull(Filename);

	return Filename;
}

/* Helper function used to generate filenames for icons */
static FString GetIconFilename(EMacImageScope::Type Scope)
{
	const FString& PlatformName = FModuleManager::GetModuleChecked<ITargetPlatformModule>("MacTargetPlatform").GetTargetPlatform()->PlatformName();

	if (Scope == EMacImageScope::Engine)
	{
		FString Filename = FPaths::EngineDir() / FString(TEXT("Source/Runtime/Launch/Resources")) / PlatformName / FString("UE4.icns");
		return FPaths::ConvertRelativePathToFull(Filename);
	}
	else
	{
		FString Filename = FPaths::GameDir() / TEXT("Build/Mac/Application.icns");
		if(!FPaths::FileExists(Filename))
		{
			FString LegacyFilename = FPaths::GameSourceDir() / FString(FApp::GetGameName()) / FString(TEXT("Resources")) / PlatformName / FString(FApp::GetGameName()) + TEXT(".icns");
			if(FPaths::FileExists(LegacyFilename))
			{
				Filename = LegacyFilename;
			}
		}
		return FPaths::ConvertRelativePathToFull(Filename);
	}
}

void FMacTargetSettingsDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	FSimpleDelegate OnUpdateShaderStandardWarning = FSimpleDelegate::CreateSP(this, &FMacTargetSettingsDetails::UpdateShaderStandardWarning);
	
	// Setup the supported/targeted RHI property view
	TargetShaderFormatsDetails = MakeShareable(new FMacShaderFormatsPropertyDetails(&DetailBuilder, TEXT("TargetedRHIs"), TEXT("Targeted RHIs")));
	TargetShaderFormatsDetails->SetOnUpdateShaderWarning(OnUpdateShaderStandardWarning);
	TargetShaderFormatsDetails->CreateTargetShaderFormatsPropertyView();
	
	// Setup the supported/targeted RHI property view
	CachedShaderFormatsDetails = MakeShareable(new FMacShaderFormatsPropertyDetails(&DetailBuilder, TEXT("CachedShaderFormats"), TEXT("Cached Shader Formats")));
	CachedShaderFormatsDetails->CreateTargetShaderFormatsPropertyView();
    
    // Setup the shader version property view
    // Handle max. shader version a little specially.
    {
        IDetailCategoryBuilder& RenderCategory = DetailBuilder.EditCategory(TEXT("Rendering"));
        ShaderVersionPropertyHandle = DetailBuilder.GetProperty(TEXT("MaxShaderLanguageVersion"));
		
		// Drop-downs for setting type of lower and upper bound normalization
		IDetailPropertyRow& ShaderVersionPropertyRow = RenderCategory.AddProperty(ShaderVersionPropertyHandle.ToSharedRef());
		ShaderVersionPropertyRow.CustomWidget()
		.NameContent()
		[
			ShaderVersionPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				SNew(SComboButton)
				.OnGetMenuContent(this, &FMacTargetSettingsDetails::OnGetShaderVersionContent)
				.ContentPadding(FMargin( 2.0f, 2.0f ))
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(this, &FMacTargetSettingsDetails::GetShaderVersionDesc)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(2)
			[
				SAssignNew(ShaderVersionWarningTextBox, SErrorText)
				.AutoWrapText(true)
			]
		];
		
		UpdateShaderStandardWarning();
    }
	
	// Add the splash image customization
	const FText EditorSplashDesc(LOCTEXT("EditorSplashLabel", "Editor Splash"));
	IDetailCategoryBuilder& SplashCategoryBuilder = DetailBuilder.EditCategory(TEXT("Splash"));
	FDetailWidgetRow& EditorSplashWidgetRow = SplashCategoryBuilder.AddCustomRow(EditorSplashDesc);

	const FString EditorSplash_TargetImagePath = GetSplashFilename(EMacImageScope::GameOverride, true);
	const FString EditorSplash_DefaultImagePath = GetSplashFilename(EMacImageScope::Engine, true);

	EditorSplashWidgetRow
	.NameContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(EditorSplashDesc)
			.Font(DetailBuilder.GetDetailFont())
		]
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SExternalImageReference, EditorSplash_DefaultImagePath, EditorSplash_TargetImagePath)
			.FileDescription(EditorSplashDesc)
			.OnGetPickerPath(FOnGetPickerPath::CreateSP(this, &FMacTargetSettingsDetails::GetPickerPath))
			.OnPostExternalImageCopy(FOnPostExternalImageCopy::CreateSP(this, &FMacTargetSettingsDetails::HandlePostExternalIconCopy))
		]
	];

	const FText GameSplashDesc(LOCTEXT("GameSplashLabel", "Game Splash"));
	FDetailWidgetRow& GameSplashWidgetRow = SplashCategoryBuilder.AddCustomRow(GameSplashDesc);

	const FString GameSplash_TargetImagePath = GetSplashFilename(EMacImageScope::GameOverride, false);
	const FString GameSplash_DefaultImagePath = GetSplashFilename(EMacImageScope::Engine, false);

	GameSplashWidgetRow
	.NameContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(GameSplashDesc)
			.Font(DetailBuilder.GetDetailFont())
		]
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SExternalImageReference, GameSplash_DefaultImagePath, GameSplash_TargetImagePath)
			.FileDescription(GameSplashDesc)
			.OnGetPickerPath(FOnGetPickerPath::CreateSP(this, &FMacTargetSettingsDetails::GetPickerPath))
			.OnPostExternalImageCopy(FOnPostExternalImageCopy::CreateSP(this, &FMacTargetSettingsDetails::HandlePostExternalIconCopy))
		]
	];

	IDetailCategoryBuilder& IconsCategoryBuilder = DetailBuilder.EditCategory(TEXT("Icon"));	
	FDetailWidgetRow& GameIconWidgetRow = IconsCategoryBuilder.AddCustomRow(LOCTEXT("GameIconLabel", "Game Icon"));
	GameIconWidgetRow
	.NameContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("GameIconLabel", "Game Icon"))
			.Font(DetailBuilder.GetDetailFont())
		]
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SExternalImageReference, GetIconFilename(EMacImageScope::Engine), GetIconFilename(EMacImageScope::GameOverride))
			.FileDescription(GameSplashDesc)
			.OnPreExternalImageCopy(FOnPreExternalImageCopy::CreateSP(this, &FMacTargetSettingsDetails::HandlePreExternalIconCopy))
			.OnGetPickerPath(FOnGetPickerPath::CreateSP(this, &FMacTargetSettingsDetails::GetPickerPath))
			.OnPostExternalImageCopy(FOnPostExternalImageCopy::CreateSP(this, &FMacTargetSettingsDetails::HandlePostExternalIconCopy))
		]
	];
}


bool FMacTargetSettingsDetails::HandlePreExternalIconCopy(const FString& InChosenImage)
{
	return true;
}


FString FMacTargetSettingsDetails::GetPickerPath()
{
	return FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN);
}


bool FMacTargetSettingsDetails::HandlePostExternalIconCopy(const FString& InChosenImage)
{
	FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_OPEN, FPaths::GetPath(InChosenImage));
	return true;
}

TSharedRef<SWidget> FMacTargetSettingsDetails::OnGetShaderVersionContent()
{
	FMenuBuilder MenuBuilder(true, NULL);
	
	UEnum* Enum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("EMacMetalShaderStandard"), true);
	
	for (int32 i = 0; i < Enum->GetMaxEnumValue(); i++)
	{
		if (Enum->IsValidEnumValue(i))
		{
			FUIAction ItemAction(FExecuteAction::CreateSP(this, &FMacTargetSettingsDetails::SetShaderStandard, i));
			MenuBuilder.AddMenuEntry(Enum->GetDisplayNameTextByValue(i), TAttribute<FText>(), FSlateIcon(), ItemAction);
		}
	}
	
	return MenuBuilder.MakeWidget();
}

FText FMacTargetSettingsDetails::GetShaderVersionDesc() const
{
	uint8 EnumValue;
	ShaderVersionPropertyHandle->GetValue(EnumValue);
	
	UEnum* Enum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("EMacMetalShaderStandard"), true);
	
	if (EnumValue < Enum->GetMaxEnumValue() && Enum->IsValidEnumValue(EnumValue))
	{
		return Enum->GetDisplayNameTextByValue(EnumValue);
	}
	
	return FText::GetEmpty();
}

void FMacTargetSettingsDetails::SetShaderStandard(int32 Value)
{
	if (Value > 1 && TargetShaderFormatsDetails->IsTargetedRHIChecked(TEXT("SF_METAL_SM4")) != ECheckBoxState::Checked)
	{
		FText Message = LOCTEXT("MacMetalShaderVersion1_2","Enabling Mac Metal Shader Standard v1.2 increases the minimum operating system requirement for Metal Shader Model 5 from OS X El Capitan 10.11.5 or later to macOS Sierra 10.12.0 or later. Either set the minimum supported OS version to 10.12.0 or enable Metal Shader Model 4 support in \"Targeted RHIs\" which will be used on OS X El Capitan.");
		ShaderVersionWarningTextBox->SetError(Message);
	}
	else
	{
		ShaderVersionWarningTextBox->SetError(TEXT(""));
	}
	FPropertyAccess::Result Res = ShaderVersionPropertyHandle->SetValue((uint8)Value);
	check(Res == FPropertyAccess::Success);
}

void FMacTargetSettingsDetails::UpdateShaderStandardWarning()
{
	// Update the UI
	uint8 EnumValue;
	ShaderVersionPropertyHandle->GetValue(EnumValue);
	SetShaderStandard(EnumValue);
}

FText GetFriendlyNameFromRHINameMac(const FString& InRHIName)
{
	FText FriendlyRHIName = LOCTEXT("UnknownRHI", "UnknownRHI");
	if (InRHIName == TEXT("GLSL_150_MAC"))
	{
		FriendlyRHIName = LOCTEXT("OpenGL3", "OpenGL 3 (SM4, Deprecated)");
	}
	else if (InRHIName == TEXT("SF_METAL_MACES3_1"))
	{
		FriendlyRHIName = LOCTEXT("MetalES3.1", "Metal (ES3.1, Mobile Preview)");
	}
	else if (InRHIName == TEXT("SF_METAL_SM4"))
	{
		FriendlyRHIName = LOCTEXT("MetalSM4", "Metal (SM4, OS X El Capitan 10.11.4 or later)");
	}
	else if (InRHIName == TEXT("SF_METAL_SM5"))
	{
		FriendlyRHIName = LOCTEXT("MetalSM5", "Metal (SM5, OS X El Capitan 10.11.5 or later)");
	}
	
	return FriendlyRHIName;
}

FMacShaderFormatsPropertyDetails::FMacShaderFormatsPropertyDetails(IDetailLayoutBuilder* InDetailBuilder, FString InProperty, FString InTitle)
: DetailBuilder(InDetailBuilder)
, Property(InProperty)
, Title(InTitle)
{
	ShaderFormatsPropertyHandle = DetailBuilder->GetProperty(*Property);
	ensure(ShaderFormatsPropertyHandle.IsValid());
}

void FMacShaderFormatsPropertyDetails::SetOnUpdateShaderWarning(FSimpleDelegate const& Delegate)
{
	ShaderFormatsPropertyHandle->SetOnPropertyValueChanged(Delegate);
}

void FMacShaderFormatsPropertyDetails::CreateTargetShaderFormatsPropertyView()
{
	DetailBuilder->HideProperty(ShaderFormatsPropertyHandle);
	
	// List of supported RHI's and selected targets
	ITargetPlatform* TargetPlatform = FModuleManager::GetModuleChecked<ITargetPlatformModule>("MacTargetPlatform").GetTargetPlatform();
	TArray<FName> ShaderFormats;
	TargetPlatform->GetAllPossibleShaderFormats(ShaderFormats);
	
	IDetailCategoryBuilder& TargetedRHICategoryBuilder = DetailBuilder->EditCategory(*Title);
	
	for (const FName& ShaderFormat : ShaderFormats)
	{
		FText FriendlyShaderFormatName = GetFriendlyNameFromRHINameMac(ShaderFormat.ToString());
		
		FDetailWidgetRow& TargetedRHIWidgetRow = TargetedRHICategoryBuilder.AddCustomRow(FriendlyShaderFormatName);
		
		TargetedRHIWidgetRow
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 1, 0, 1))
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(FriendlyShaderFormatName)
				.Font(DetailBuilder->GetDetailFont())
			 ]
		 ]
		.ValueContent()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &FMacShaderFormatsPropertyDetails::OnTargetedRHIChanged, ShaderFormat)
			.IsChecked(this, &FMacShaderFormatsPropertyDetails::IsTargetedRHIChecked, ShaderFormat)
		 ];
	}
}


void FMacShaderFormatsPropertyDetails::OnTargetedRHIChanged(ECheckBoxState InNewValue, FName InRHIName)
{
	TArray<void*> RawPtrs;
	ShaderFormatsPropertyHandle->AccessRawData(RawPtrs);
	
	// Update the CVars with the selection
	{
		ShaderFormatsPropertyHandle->NotifyPreChange();
		for (void* RawPtr : RawPtrs)
		{
			TArray<FString>& Array = *(TArray<FString>*)RawPtr;
			if(InNewValue == ECheckBoxState::Checked)
			{
				Array.Add(InRHIName.ToString());
			}
			else
			{
				Array.Remove(InRHIName.ToString());
			}
		}
		ShaderFormatsPropertyHandle->NotifyPostChange();
	}
}


ECheckBoxState FMacShaderFormatsPropertyDetails::IsTargetedRHIChecked(FName InRHIName) const
{
	ECheckBoxState CheckState = ECheckBoxState::Unchecked;
	
	TArray<void*> RawPtrs;
	ShaderFormatsPropertyHandle->AccessRawData(RawPtrs);
	
	for(void* RawPtr : RawPtrs)
	{
		TArray<FString>& Array = *(TArray<FString>*)RawPtr;
		if(Array.Contains(InRHIName.ToString()))
		{
			CheckState = ECheckBoxState::Checked;
		}
	}
	return CheckState;
}

#undef LOCTEXT_NAMESPACE
