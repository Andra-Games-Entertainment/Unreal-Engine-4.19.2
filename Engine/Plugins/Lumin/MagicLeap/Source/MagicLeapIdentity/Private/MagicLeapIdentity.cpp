// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

#include "MagicLeapIdentity.h"
#include "IMagicLeapIdentityPlugin.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "MagicLeapPluginUtil.h"
#include "ml_identity.h"

DEFINE_LOG_CATEGORY_STATIC(LogMagicLeapIdentity, Display, All);

class FMagicLeapIdentityPlugin : public IMagicLeapIdentityPlugin
{
public:
	void StartupModule() override
	{
		IModuleInterface::StartupModule();
		APISetup.Startup();
		APISetup.LoadDLL(TEXT("ml_identity"));
	}

	void ShutdownModule() override
	{
		APISetup.Shutdown();
		IModuleInterface::ShutdownModule();
	}

private:
	FMagicLeapAPISetup APISetup;
};

IMPLEMENT_MODULE(FMagicLeapIdentityPlugin, MagicLeapIdentity);

//////////////////////////////////////////////////////////////////////////

class FIdentityImpl
{
public:
	struct FRequestAttribData
	{
	public:
		FRequestAttribData()
		{}

		FRequestAttribData(MLIdentityProfile* profile, const UMagicLeapIdentity::FRequestIdentityAttributeValueDelegate& requestDelegate)
			: Profile(profile)
			, RequestDelegate(requestDelegate)
		{}

		MLIdentityProfile* Profile;
		UMagicLeapIdentity::FRequestIdentityAttributeValueDelegate RequestDelegate;
	};

	struct FModifyAttribData
	{
	public:
		FModifyAttribData()
		{}

		FModifyAttribData(MLIdentityProfile* profile, const UMagicLeapIdentity::FModifyIdentityAttributeValueDelegate& requestDelegate)
			: Profile(profile)
			, RequestDelegate(requestDelegate)
		{}

		MLIdentityProfile* Profile;
		UMagicLeapIdentity::FModifyIdentityAttributeValueDelegate RequestDelegate;
	};

	EMagicLeapIdentityError MLToUnrealIdentityError(MLIdentityError error)
	{
		switch (error)
		{
		case MLIdentityError_Ok:
			return EMagicLeapIdentityError::Ok;
		case MLIdentityError_FailedToConnectToLocalService:
			return EMagicLeapIdentityError::FailedToConnectToLocalService;
		case MLIdentityError_FailedToConnectToCloudService:
			return EMagicLeapIdentityError::FailedToConnectToCloudService;
		case MLIdentityError_CloudAuthentication:
			return EMagicLeapIdentityError::CloudAuthentication;
		case MLIdentityError_InvalidInformationFromCloud:
			return EMagicLeapIdentityError::InvalidInformationFromCloud;
		case MLIdentityError_InvalidArgument:
			return EMagicLeapIdentityError::InvalidArgument;
		case MLIdentityError_AsyncOperationNotComplete:
			return EMagicLeapIdentityError::AsyncOperationNotComplete;
		case MLIdentityError_OtherError:
			return EMagicLeapIdentityError::OtherError;
		}

		return EMagicLeapIdentityError::OtherError;
	}

	EMagicLeapIdentityAttribute MLToUnrealIdentityAttribute(MLIdentityAttributeEnum attribute)
	{
		switch (attribute)
		{
		case MLIdentityAttributeEnum_UserId:
			return EMagicLeapIdentityAttribute::UserID;
		case MLIdentityAttributeEnum_GivenName:
			return EMagicLeapIdentityAttribute::GivenName;
		case MLIdentityAttributeEnum_FamilyName:
			return EMagicLeapIdentityAttribute::FamilyName;
		case MLIdentityAttributeEnum_Email:
			return EMagicLeapIdentityAttribute::Email;
		case MLIdentityAttributeEnum_Status:
			return EMagicLeapIdentityAttribute::Status;
		case MLIdentityAttributeEnum_TermsAccepted:
			return EMagicLeapIdentityAttribute::TermsAccepted;
		case MLIdentityAttributeEnum_Birthday:
			return EMagicLeapIdentityAttribute::Birthday;
		case MLIdentityAttributeEnum_Company:
			return EMagicLeapIdentityAttribute::Company;
		case MLIdentityAttributeEnum_Industry:
			return EMagicLeapIdentityAttribute::Industry;
		case MLIdentityAttributeEnum_Location:
			return EMagicLeapIdentityAttribute::Location;
		case MLIdentityAttributeEnum_Tagline:
			return EMagicLeapIdentityAttribute::Tagline;
		case MLIdentityAttributeEnum_PhoneNumber:
			return EMagicLeapIdentityAttribute::PhoneNumber;
		case MLIdentityAttributeEnum_Avatar2D:
			return EMagicLeapIdentityAttribute::Avatar2D;
		case MLIdentityAttributeEnum_Avatar3D:
			return EMagicLeapIdentityAttribute::Avatar3D;
		case MLIdentityAttributeEnum_IsDeveloper:
			return EMagicLeapIdentityAttribute::IsDeveloper;
		case MLIdentityAttributeEnum_Unknown:
			return EMagicLeapIdentityAttribute::Unknown;
		}
		return EMagicLeapIdentityAttribute::Unknown;
	}

	MLIdentityAttributeEnum UnrealToMLIdentityAttribute(EMagicLeapIdentityAttribute attribute)
	{
		switch (attribute)
		{
		case EMagicLeapIdentityAttribute::UserID:
			return MLIdentityAttributeEnum_UserId;
		case EMagicLeapIdentityAttribute::GivenName:
			return MLIdentityAttributeEnum_GivenName;
		case EMagicLeapIdentityAttribute::FamilyName:
			return MLIdentityAttributeEnum_FamilyName;
		case EMagicLeapIdentityAttribute::Email:
			return MLIdentityAttributeEnum_Email;
		case EMagicLeapIdentityAttribute::Status:
			return MLIdentityAttributeEnum_Status;
		case EMagicLeapIdentityAttribute::TermsAccepted:
			return MLIdentityAttributeEnum_TermsAccepted;
		case EMagicLeapIdentityAttribute::Birthday:
			return MLIdentityAttributeEnum_Birthday;
		case EMagicLeapIdentityAttribute::Company:
			return MLIdentityAttributeEnum_Company;
		case EMagicLeapIdentityAttribute::Industry:
			return MLIdentityAttributeEnum_Industry;
		case EMagicLeapIdentityAttribute::Location:
			return MLIdentityAttributeEnum_Location;
		case EMagicLeapIdentityAttribute::Tagline:
			return MLIdentityAttributeEnum_Tagline;
		case EMagicLeapIdentityAttribute::PhoneNumber:
			return MLIdentityAttributeEnum_PhoneNumber;
		case EMagicLeapIdentityAttribute::Avatar2D:
			return MLIdentityAttributeEnum_Avatar2D;
		case EMagicLeapIdentityAttribute::Avatar3D:
			return MLIdentityAttributeEnum_Avatar3D;
		case EMagicLeapIdentityAttribute::IsDeveloper:
			return MLIdentityAttributeEnum_IsDeveloper;
		case EMagicLeapIdentityAttribute::Unknown:
			return MLIdentityAttributeEnum_Unknown;
		}
		return MLIdentityAttributeEnum_Unknown;
	}


	TMap<MLInvokeFuture*, UMagicLeapIdentity::FAvailableIdentityAttributesDelegate> AllAvailableAttribsFutures;
	TMap<MLInvokeFuture*, FRequestAttribData> AllRequestAttribsFutures;
	TMap<MLInvokeFuture*, FModifyAttribData> AllModifyAttribsFutures;
};

UMagicLeapIdentity::UMagicLeapIdentity()
	: Impl(new FIdentityImpl())
{}

UMagicLeapIdentity::~UMagicLeapIdentity()
{
	delete Impl;
}

EMagicLeapIdentityError UMagicLeapIdentity::GetAllAvailableAttributes(TArray<EMagicLeapIdentityAttribute>& AvailableAttributes)
{
	MLIdentityProfile* profile = nullptr;
	MLIdentityError result = MLIdentityGetAttributeNames(&profile);

	AvailableAttributes.Empty();
	if (result == MLIdentityError_Ok && profile != nullptr)
	{
		for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
		{
			const MLIdentityAttribute* attribute = profile->attribute_ptrs[i];
			AvailableAttributes.Add(Impl->MLToUnrealIdentityAttribute(attribute->enum_value));
		}

		MLIdentityReleaseUserProfile(profile);
	}

	return Impl->MLToUnrealIdentityError(result);
}

void UMagicLeapIdentity::GetAllAvailableAttributesAsync(const FAvailableIdentityAttributesDelegate& ResultDelegate)
{
	Impl->AllAvailableAttribsFutures.Add(MLIdentityGetAttributeNamesAsync(), ResultDelegate);
}

EMagicLeapIdentityError UMagicLeapIdentity::RequestAttributeValue(const TArray<EMagicLeapIdentityAttribute>& Attribute, TArray<FMagicLeapIdentityAttribute>& AttributeValue)
{
	MLIdentityProfile* profile = nullptr;
	TArray<MLIdentityAttributeEnum> mlAttributes;
	for (EMagicLeapIdentityAttribute attrib : Attribute)
	{
		mlAttributes.Add(Impl->UnrealToMLIdentityAttribute(attrib));
	}
	MLIdentityError result = MLIdentityGetKnownAttributeNames(mlAttributes.GetData(), mlAttributes.Num(), &profile);

	AttributeValue.Empty();
	if (result == MLIdentityError_Ok && profile != nullptr)
	{
		for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
		{
			profile->attribute_ptrs[i]->is_requested = true;
		}

		result = MLIdentityRequestAttributeValues(profile);

		if (result == MLIdentityError_Ok)
		{
			for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
			{
				if (profile->attribute_ptrs[i]->is_granted)
				{
					AttributeValue.Add(FMagicLeapIdentityAttribute(Impl->MLToUnrealIdentityAttribute(profile->attribute_ptrs[i]->enum_value), FString(ANSI_TO_TCHAR(profile->attribute_ptrs[i]->value))));
				}
			}
		}

		MLIdentityReleaseUserProfile(profile);
	}

	return Impl->MLToUnrealIdentityError(result);
}

EMagicLeapIdentityError UMagicLeapIdentity::RequestAttributeValueAsync(const TArray<EMagicLeapIdentityAttribute>& Attribute, const FRequestIdentityAttributeValueDelegate& ResultDelegate)
{
	MLIdentityProfile* profile = nullptr;
	TArray<MLIdentityAttributeEnum> mlAttributes;
	for (EMagicLeapIdentityAttribute attrib : Attribute)
	{
		mlAttributes.Add(Impl->UnrealToMLIdentityAttribute(attrib));
	}
	MLIdentityError result = MLIdentityGetKnownAttributeNames(mlAttributes.GetData(), mlAttributes.Num(), &profile);

	if (result == MLIdentityError_Ok && profile != nullptr)
	{
		for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
		{
			profile->attribute_ptrs[i]->is_requested = true;
		}

		Impl->AllRequestAttribsFutures.Add(MLIdentityRequestAttributeValuesAsync(profile), FIdentityImpl::FRequestAttribData(profile, ResultDelegate));
	}

	return Impl->MLToUnrealIdentityError(result);
}

EMagicLeapIdentityError UMagicLeapIdentity::ModifyAttributeValue(const TArray<FMagicLeapIdentityAttribute>& UpdatedAttributeValue, TArray<EMagicLeapIdentityAttribute>& AttributesUpdatedSuccessfully)
{
	MLIdentityProfile* profile = nullptr;
	TArray<MLIdentityAttributeEnum> requestedAttributes;
	for (const auto& updatedAttribute : UpdatedAttributeValue)
	{
		requestedAttributes.Add(Impl->UnrealToMLIdentityAttribute(updatedAttribute.Attribute));
	}
	MLIdentityError result = MLIdentityGetKnownAttributeNames(requestedAttributes.GetData(), requestedAttributes.Num(), &profile);

	AttributesUpdatedSuccessfully.Empty();
	if (result == MLIdentityError_Ok && profile != nullptr)
	{
		for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
		{
			profile->attribute_ptrs[i]->is_requested = true;
		}

		result = MLIdentityModifyAttributeValues(profile);

		if (result == MLIdentityError_Ok)
		{
			for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
			{
				if (profile->attribute_ptrs[i]->is_granted)
				{
					AttributesUpdatedSuccessfully.Add(Impl->MLToUnrealIdentityAttribute(profile->attribute_ptrs[i]->enum_value));
				}
			}
		}

		MLIdentityReleaseUserProfile(profile);
	}

	return Impl->MLToUnrealIdentityError(result);
}

EMagicLeapIdentityError UMagicLeapIdentity::ModifyAttributeValueAsync(const TArray<FMagicLeapIdentityAttribute>& UpdatedAttributeValue, const FModifyIdentityAttributeValueDelegate& ResultDelegate)
{
	MLIdentityProfile* profile = nullptr;
	TArray<MLIdentityAttributeEnum> requestedAttributes;
	for (const auto& updatedAttribute : UpdatedAttributeValue)
	{
		requestedAttributes.Add(Impl->UnrealToMLIdentityAttribute(updatedAttribute.Attribute));
	}
	MLIdentityError result = MLIdentityGetKnownAttributeNames(requestedAttributes.GetData(), requestedAttributes.Num(), &profile);

	if (result == MLIdentityError_Ok && profile != nullptr)
	{
		for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
		{
			profile->attribute_ptrs[i]->is_requested = true;
		}

		Impl->AllModifyAttribsFutures.Add(MLIdentityRequestAttributeValuesAsync(profile), FIdentityImpl::FModifyAttribData(profile, ResultDelegate));
	}

	return Impl->MLToUnrealIdentityError(result);
}

UWorld* UMagicLeapIdentity::GetWorld() const
{
	return Cast<UWorld>(GetOuter());
}

void UMagicLeapIdentity::Tick(float DeltaTime)
{
	TArray<MLInvokeFuture*> FuturesToDelete;

	// MLIdentityGetAttributeNamesAsync()
	for (const auto& AvailableAttribsFuture : Impl->AllAvailableAttribsFutures)
	{
		MLInvokeFuture* future = AvailableAttribsFuture.Key;
		MLIdentityError error = MLIdentityError_OtherError;
		MLIdentityProfile* profile = nullptr;

		if (MLIdentityGetAttributeNamesWait(future, 0, &error, &profile))
		{
			FuturesToDelete.Add(future);
			TArray<EMagicLeapIdentityAttribute> AvailableAttributes;
			if (error == MLIdentityError_Ok && profile != nullptr)
			{
				for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
				{
					const MLIdentityAttribute* attribute = profile->attribute_ptrs[i];
					AvailableAttributes.Add(Impl->MLToUnrealIdentityAttribute(attribute->enum_value));
				}
				MLIdentityReleaseUserProfile(profile);
			}
			AvailableAttribsFuture.Value.ExecuteIfBound(Impl->MLToUnrealIdentityError(error), AvailableAttributes);
		}
	}

	for (const MLInvokeFuture* future : FuturesToDelete)
	{
		Impl->AllAvailableAttribsFutures.Remove(future);
	}

	FuturesToDelete.Empty();

	// MLIdentityRequestAttributeValuesAsync()
	for (const auto& RequestAttribsFuture : Impl->AllRequestAttribsFutures)
	{
		MLInvokeFuture* future = RequestAttribsFuture.Key;
		MLIdentityError error = MLIdentityError_OtherError;

		if (MLIdentityRequestAttributeValuesWait(future, 0, &error))
		{
			FuturesToDelete.Add(future);
			TArray<FMagicLeapIdentityAttribute> AttributeValue;
			MLIdentityProfile* profile = RequestAttribsFuture.Value.Profile;

			if (error == MLIdentityError_Ok)
			{
				for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
				{
					if (profile->attribute_ptrs[i]->is_granted)
					{
						AttributeValue.Add(FMagicLeapIdentityAttribute(Impl->MLToUnrealIdentityAttribute(profile->attribute_ptrs[i]->enum_value), FString(ANSI_TO_TCHAR(profile->attribute_ptrs[i]->value))));
					}
				}
			}
			RequestAttribsFuture.Value.RequestDelegate.ExecuteIfBound(Impl->MLToUnrealIdentityError(error), AttributeValue);
			MLIdentityReleaseUserProfile(profile);
		}
	}

	for (const MLInvokeFuture* future : FuturesToDelete)
	{
		Impl->AllRequestAttribsFutures.Remove(future);
	}

	FuturesToDelete.Empty();

	// MLIdentityModifyAttributeValuesAsync()
	for (const auto& ModifyAttribsFuture : Impl->AllModifyAttribsFutures)
	{
		MLInvokeFuture* future = ModifyAttribsFuture.Key;
		MLIdentityError error = MLIdentityError_OtherError;

		if (MLIdentityModifyAttributeValuesWait(future, 0, &error))
		{
			FuturesToDelete.Add(future);
			TArray<EMagicLeapIdentityAttribute> AttributesUpdatedSuccessfully;
			MLIdentityProfile* profile = ModifyAttribsFuture.Value.Profile;

			if (error == MLIdentityError_Ok)
			{
				for (uint32 i = 0; i < static_cast<uint32>(profile->attribute_count); ++i)
				{
					if (profile->attribute_ptrs[i]->is_granted)
					{
						AttributesUpdatedSuccessfully.Add(Impl->MLToUnrealIdentityAttribute(profile->attribute_ptrs[i]->enum_value));
					}
				}
			}
			ModifyAttribsFuture.Value.RequestDelegate.ExecuteIfBound(Impl->MLToUnrealIdentityError(error), AttributesUpdatedSuccessfully);
			MLIdentityReleaseUserProfile(profile);
		}
	}

	for (const MLInvokeFuture* future : FuturesToDelete)
	{
		Impl->AllModifyAttribsFutures.Remove(future);
	}

	FuturesToDelete.Empty();
}

bool UMagicLeapIdentity::IsTickable() const
{
	return HasAnyFlags(RF_ClassDefaultObject) == false;
}

TStatId UMagicLeapIdentity::GetStatId() const
{
	return GetStatID(false);
}

UWorld* UMagicLeapIdentity::GetTickableGameObjectWorld() const
{
	return GetWorld();
}
