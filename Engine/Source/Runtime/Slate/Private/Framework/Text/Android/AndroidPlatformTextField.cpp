// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Framework/Text/Android/AndroidPlatformTextField.h"
#include "Widgets/Input/IVirtualKeyboardEntry.h"
#include "Misc/CommandLine.h"

// Java InputType class
#define TYPE_CLASS_TEXT						0x00000001
#define TYPE_CLASS_NUMBER					0x00000002

// Java InputType number flags
#define TYPE_NUMBER_FLAG_SIGNED				0x00001000
#define TYPE_NUMBER_FLAG_DECIMAL			0x00002000

// Java InputType text variation flags
#define TYPE_TEXT_VARIATION_EMAIL_ADDRESS	0x00000020
#define TYPE_TEXT_VARIATION_NORMAL			0x00000000
#define TYPE_TEXT_VARIATION_PASSWORD		0x00000080
#define TYPE_TEXT_VARIATION_URI				0x00000010

// Java InputType text flags
#define TYPE_TEXT_FLAG_NO_SUGGESTIONS		0x00080000


void FAndroidPlatformTextField::ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget)
{
	// Set the EditBox inputType based on keyboard type
	int32 InputType;
	
	if (bShow)
	{
		switch (TextEntryWidget->GetVirtualKeyboardType())
		{
			case EKeyboardType::Keyboard_Number:
				InputType = TYPE_CLASS_NUMBER | TYPE_NUMBER_FLAG_SIGNED | TYPE_NUMBER_FLAG_DECIMAL;
				break;
			case EKeyboardType::Keyboard_Web:
				InputType = TYPE_CLASS_TEXT | TYPE_TEXT_VARIATION_URI;
				break;
			case EKeyboardType::Keyboard_Email:
				InputType = TYPE_CLASS_TEXT | TYPE_TEXT_VARIATION_EMAIL_ADDRESS;
				break;
			case EKeyboardType::Keyboard_Password:
				InputType = TYPE_CLASS_TEXT | TYPE_TEXT_VARIATION_PASSWORD;
				break;
			case EKeyboardType::Keyboard_AlphaNumeric:
			case EKeyboardType::Keyboard_Default:
			default:
				InputType = TYPE_CLASS_TEXT | TYPE_TEXT_VARIATION_NORMAL;
				break;
		}
		
		// Do not make suggestions as user types
		InputType |= TYPE_TEXT_FLAG_NO_SUGGESTIONS;
	}
	
	// This option is tied to a command line parameter (same as IOS)
	static int IsUsingIntegratedKeyboard = -1;
	if (IsUsingIntegratedKeyboard == -1)
	{
		IsUsingIntegratedKeyboard = FParse::Param(FCommandLine::Get(), TEXT("NewKeyboard")) ? 1 : 0;
	}
	
	if (IsUsingIntegratedKeyboard > 0)
	{
		if (bShow)
		{
			// Show alert for input
			extern void AndroidThunkCpp_ShowVirtualKeyboardInput(TSharedPtr<IVirtualKeyboardEntry>, int32, const FString&, const FString&);
			AndroidThunkCpp_ShowVirtualKeyboardInput(TextEntryWidget, InputType, TextEntryWidget->GetHintText().ToString(), TextEntryWidget->GetText().ToString());
		}
		else
		{
			extern void AndroidThunkCpp_HideVirtualKeyboardInput();
			AndroidThunkCpp_HideVirtualKeyboardInput();
		}
	}
	else
	{
		if (bShow)
		{
			// Show alert for input
			extern void AndroidThunkCpp_ShowVirtualKeyboardInputDialog(TSharedPtr<IVirtualKeyboardEntry>, int32, const FString&, const FString&);
			AndroidThunkCpp_ShowVirtualKeyboardInputDialog(TextEntryWidget, InputType, TextEntryWidget->GetHintText().ToString(), TextEntryWidget->GetText().ToString());
		}
		else
		{
			extern void AndroidThunkCpp_HideVirtualKeyboardInputDialog();
			AndroidThunkCpp_HideVirtualKeyboardInputDialog();
		}
	}
}
