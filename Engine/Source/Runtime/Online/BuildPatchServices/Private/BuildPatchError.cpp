// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchError.cpp: Implements classes involved setting and getting error information.
=============================================================================*/

#include "BuildPatchError.h"
#include "Misc/ScopeLock.h"
#include "Interfaces/IBuildInstaller.h"
#include "BuildPatchServicesPrivate.h"

#define LOCTEXT_NAMESPACE "BuildPatchInstallError"

namespace BuildPatchInstallError
{
	const FText& GetStandardErrorText(EBuildPatchInstallError ErrorType)
	{
		// The generic texts, for when not specified by the system reporting the error.
		static const FText NoError(LOCTEXT("BuildPatchInstallShortError_NoError", "The operation was successful."));
		static const FText DownloadError(LOCTEXT("BuildPatchInstallShortError_DownloadError", "Could not download patch data. Please try again later."));
		static const FText FileConstructionFail(LOCTEXT("BuildPatchInstallShortError_FileConstructionFail", "A file corruption has occurred. Please try again."));
		static const FText MoveFileToInstall(LOCTEXT("BuildPatchInstallShortError_MoveFileToInstall", "A file access error has occurred. Please check your running processes."));
		static const FText BuildVerifyFail(LOCTEXT("BuildPatchInstallShortError_BuildCorrupt", "The installation is corrupt. Please contact support."));
		static const FText ApplicationClosing(LOCTEXT("BuildPatchInstallShortError_ApplicationClosing", "The application is closing."));
		static const FText ApplicationError(LOCTEXT("BuildPatchInstallShortError_ApplicationError", "Patching service could not start. Please contact support."));
		static const FText UserCanceled(LOCTEXT("BuildPatchInstallShortError_UserCanceled", "User cancelled."));
		static const FText PrerequisiteError(LOCTEXT("BuildPatchInstallShortError_PrerequisiteError", "The necessary prerequisites have failed to install. Please contact support."));
		static const FText InitializationError(LOCTEXT("BuildPatchInstallShortError_InitializationError", "The installer failed to initialize. Please contact support."));
		static const FText PathLengthExceeded(LOCTEXT("BuildPatchInstallShortError_PathLengthExceeded", "Maximum path length exceeded. Please specify a shorter install location."));
		static const FText OutOfDiskSpace(LOCTEXT("BuildPatchInstallShortError_OutOfDiskSpace", "Not enough disk space available. Please free up some disk space and try again."));
		static const FText InvalidOrMax(LOCTEXT("BuildPatchInstallShortError_InvalidOrMax", "An unknown error ocurred. Please contact support."));

		switch (ErrorType)
		{
			case EBuildPatchInstallError::NoError: return NoError;
			case EBuildPatchInstallError::DownloadError: return DownloadError;
			case EBuildPatchInstallError::FileConstructionFail: return FileConstructionFail;
			case EBuildPatchInstallError::MoveFileToInstall: return MoveFileToInstall;
			case EBuildPatchInstallError::BuildVerifyFail: return BuildVerifyFail;
			case EBuildPatchInstallError::ApplicationClosing: return ApplicationClosing;
			case EBuildPatchInstallError::ApplicationError: return ApplicationError;
			case EBuildPatchInstallError::UserCanceled: return UserCanceled;
			case EBuildPatchInstallError::PrerequisiteError: return PrerequisiteError;
			case EBuildPatchInstallError::InitializationError: return InitializationError;
			case EBuildPatchInstallError::PathLengthExceeded: return PathLengthExceeded;
			case EBuildPatchInstallError::OutOfDiskSpace: return OutOfDiskSpace;
			default: return InvalidOrMax;
		}
	}
}

/* FBuildPatchInstallError implementation
*****************************************************************************/
void FBuildPatchInstallError::Reset()
{
	FScopeLock ScopeLock( &ThreadLock );
	ErrorType = EBuildPatchInstallError::NoError;
	ErrorCode = InstallErrorPrefixes::ErrorTypeStrings[static_cast<int32>(ErrorType)];
	ErrorText = BuildPatchInstallError::GetStandardErrorText(ErrorType);
}

bool FBuildPatchInstallError::HasFatalError()
{
	FScopeLock ScopeLock( &ThreadLock );
	return ErrorType != EBuildPatchInstallError::NoError;
}

EBuildPatchInstallError FBuildPatchInstallError::GetErrorState()
{
	FScopeLock ScopeLock( &ThreadLock );
	return ErrorType;
}

bool FBuildPatchInstallError::IsInstallationCancelled()
{
	FScopeLock ScopeLock( &ThreadLock );
	return ErrorType == EBuildPatchInstallError::UserCanceled || ErrorType == EBuildPatchInstallError::ApplicationClosing;
}

bool FBuildPatchInstallError::IsNoRetryError()
{
	FScopeLock ScopeLock( &ThreadLock );
	return ErrorType == EBuildPatchInstallError::DownloadError
		|| ErrorType == EBuildPatchInstallError::MoveFileToInstall
		|| ErrorType == EBuildPatchInstallError::InitializationError
		|| ErrorType == EBuildPatchInstallError::PathLengthExceeded
		|| ErrorType == EBuildPatchInstallError::OutOfDiskSpace;
}

FText FBuildPatchInstallError::GetErrorText()
{
	FScopeLock ScopeLock(&ThreadLock);
	return ErrorText;
}

FString FBuildPatchInstallError::GetErrorCode()
{
	FScopeLock ScopeLock(&ThreadLock);
	return ErrorCode;
}

void FBuildPatchInstallError::SetFatalError( const EBuildPatchInstallError& InErrorType, const FString& InErrorCode, const FText& InErrorText )
{
	// Don't set NoError, use Reset.
	check(InErrorType != EBuildPatchInstallError::NoError);
	FScopeLock ScopeLock( &ThreadLock );
	// Only accept the first error
	if (HasFatalError())
	{
		return;
	}
	ErrorType = InErrorType;
	ErrorCode = InstallErrorPrefixes::ErrorTypeStrings[static_cast<int32>(InErrorType)] + InErrorCode;
	ErrorText = InErrorText.IsEmpty() ? BuildPatchInstallError::GetStandardErrorText(InErrorType) : InErrorText;
	UE_LOG(LogBuildPatchServices, Error, TEXT("%s %s"), *EnumToString(ErrorType), *ErrorCode);
}

const FString& FBuildPatchInstallError::EnumToString( const EBuildPatchInstallError& InErrorType )
{
	// Const enum strings, special case no error.
	static const FString NoError( "SUCCESS" );
	static const FString DownloadError( "EBuildPatchInstallError::DownloadError" );
	static const FString FileConstructionFail( "EBuildPatchInstallError::FileConstructionFail" );
	static const FString MoveFileToInstall( "EBuildPatchInstallError::MoveFileToInstall" );
	static const FString BuildVerifyFail( "EBuildPatchInstallError::BuildVerifyFail" );
	static const FString ApplicationClosing( "EBuildPatchInstallError::ApplicationClosing" );
	static const FString ApplicationError( "EBuildPatchInstallError::ApplicationError" );
	static const FString UserCanceled( "EBuildPatchInstallError::UserCanceled" );
	static const FString PrerequisiteError( "EBuildPatchInstallError::PrerequisiteError" );
	static const FString InitializationError("EBuildPatchInstallError::InitializationError");
	static const FString PathLengthExceeded("EBuildPatchInstallError::PathLengthExceeded");
	static const FString OutOfDiskSpace("EBuildPatchInstallError::OutOfDiskSpace");
	static const FString InvalidOrMax( "EBuildPatchInstallError::InvalidOrMax" );

	FScopeLock ScopeLock( &ThreadLock );
	switch ( InErrorType )
	{
		case EBuildPatchInstallError::NoError: return NoError;
		case EBuildPatchInstallError::DownloadError: return DownloadError;
		case EBuildPatchInstallError::FileConstructionFail: return FileConstructionFail;
		case EBuildPatchInstallError::MoveFileToInstall: return MoveFileToInstall;
		case EBuildPatchInstallError::BuildVerifyFail: return BuildVerifyFail;
		case EBuildPatchInstallError::ApplicationClosing: return ApplicationClosing;
		case EBuildPatchInstallError::ApplicationError: return ApplicationError;
		case EBuildPatchInstallError::UserCanceled: return UserCanceled;
		case EBuildPatchInstallError::PrerequisiteError: return PrerequisiteError;
		case EBuildPatchInstallError::InitializationError: return InitializationError;
		case EBuildPatchInstallError::PathLengthExceeded: return PathLengthExceeded;
		case EBuildPatchInstallError::OutOfDiskSpace: return OutOfDiskSpace;
		default: return InvalidOrMax;
	}
}

FText FBuildPatchInstallError::GetDiskSpaceMessage(const FString& Location, uint64 RequiredBytes, uint64 AvailableBytes, const FNumberFormattingOptions* FormatOptions)
{
	static const FText OutOfDiskSpace(LOCTEXT("InstallDirectoryDiskSpace", "There is not enough space at {Location}\n{RequiredBytes} is required.\n{AvailableBytes} is available.\nYou need an additional {SpaceAdditional} to perform the installation."));
	static const FNumberFormattingOptions DefaultOptions = FNumberFormattingOptions()
		.SetMinimumFractionalDigits(2)
		.SetMaximumFractionalDigits(2);
	FormatOptions = FormatOptions == nullptr ? &DefaultOptions : FormatOptions;
	FFormatNamedArguments Arguments;
	Arguments.Emplace(TEXT("Location"), FText::FromString(Location));
	Arguments.Emplace(TEXT("RequiredBytes"), FText::AsMemory(RequiredBytes, FormatOptions));
	Arguments.Emplace(TEXT("AvailableBytes"), FText::AsMemory(AvailableBytes, FormatOptions));
	Arguments.Emplace(TEXT("SpaceAdditional"), FText::AsMemory(RequiredBytes - AvailableBytes, FormatOptions));
	return FText::Format(OutOfDiskSpace, Arguments);
}

/**
 * Static FBuildPatchInstallError variables
 */
FCriticalSection FBuildPatchInstallError::ThreadLock;
EBuildPatchInstallError FBuildPatchInstallError::ErrorType = EBuildPatchInstallError::NoError;
FString FBuildPatchInstallError::ErrorCode;
FText FBuildPatchInstallError::ErrorText;

#undef LOCTEXT_NAMESPACE

