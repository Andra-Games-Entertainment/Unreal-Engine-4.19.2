// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemNullPrivatePCH.h"
#include "OnlineIdentityNull.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"

bool FUserOnlineAccountNull::GetAuthAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	const FString* FoundAttr = AdditionalAuthData.Find(AttrName);
	if (FoundAttr != NULL)
	{
		OutAttrValue = *FoundAttr;
		return true;
	}
	return false;
}

bool FUserOnlineAccountNull::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	const FString* FoundAttr = UserAttributes.Find(AttrName);
	if (FoundAttr != NULL)
	{
		OutAttrValue = *FoundAttr;
		return true;
	}
	return false;
}

inline FString GenerateRandomUserId(int32 LocalUserNum)
{
	FString HostName;
	if (!ISocketSubsystem::Get()->GetHostName(HostName))
	{
		// could not get hostname, use address
		bool bCanBindAll;
		TSharedPtr<class FInternetAddr> Addr = ISocketSubsystem::Get()->GetLocalHostAddr(*GLog, bCanBindAll);
		HostName = Addr->ToString(false);
	}

	return FString::Printf(TEXT("%s-%s"), *HostName, *FGuid::NewGuid().ToString());
}

bool FOnlineIdentityNull::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	FString ErrorStr;
	TSharedPtr<FUserOnlineAccountNull> UserAccountPtr;
	
	// valid local player index
	if (LocalUserNum < 0 || LocalUserNum >= MAX_LOCAL_PLAYERS)
	{
		ErrorStr = FString::Printf(TEXT("Invalid LocalUserNum=%d"), LocalUserNum);
	}
	else if (AccountCredentials.Id.IsEmpty())
	{
		ErrorStr = FString::Printf(TEXT("Invalid account id=%s"), *AccountCredentials.Id);
	}
	else
	{
		TSharedPtr<FUniqueNetId>* UserId = UserIds.Find(LocalUserNum);
		if (UserId == NULL)
		{
			FString RandomUserId = GenerateRandomUserId(LocalUserNum);

			FUniqueNetIdString NewUserId(RandomUserId);
			UserAccountPtr = MakeShareable(new FUserOnlineAccountNull(RandomUserId));
			UserAccountPtr->UserAttributes.Add(TEXT("id"), RandomUserId);

			// update/add cached entry for user
			UserAccounts.Add(NewUserId, UserAccountPtr.ToSharedRef());

			// keep track of user ids for local users
			UserIds.Add(LocalUserNum, UserAccountPtr->GetUserId());
		}
		else
		{
			const FUniqueNetIdString* UniqueIdStr = (FUniqueNetIdString*)(UserId->Get());
			TSharedRef<FUserOnlineAccountNull>* TempPtr = UserAccounts.Find(*UniqueIdStr);
			check(TempPtr);
			UserAccountPtr = *TempPtr;
		}
	}

	if (!ErrorStr.IsEmpty())
	{
		UE_LOG_ONLINE(Warning, TEXT("Login request failed. %s"), *ErrorStr);
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, FUniqueNetIdString(), ErrorStr);
		return false;
	}

	TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UserAccountPtr->GetUserId(), ErrorStr);
	return true;
}

bool FOnlineIdentityNull::Logout(int32 LocalUserNum)
{
	TSharedPtr<FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
	if (UserId.IsValid())
	{
		// remove cached user account
		UserAccounts.Remove(FUniqueNetIdString(*UserId));
		// remove cached user id
		UserIds.Remove(LocalUserNum);
		// not async but should call completion delegate anyway
		TriggerOnLogoutCompleteDelegates(LocalUserNum, true);

		return true;
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("No logged in user found for LocalUserNum=%d."),
			LocalUserNum);
		TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
	}
	return false;
}

bool FOnlineIdentityNull::AutoLogin(int32 LocalUserNum)
{
	FString LoginStr;
	FString PasswordStr;
	FString TypeStr;

	FParse::Value(FCommandLine::Get(), TEXT("LOGIN="), LoginStr);
	if (!LoginStr.IsEmpty())
	{
		UE_LOG_ONLINE(Warning, TEXT("LOGIN commandline parameter is deprecated, use AUTH_LOGIN"));
	}
	else
	{
		FParse::Value(FCommandLine::Get(), TEXT("AUTH_LOGIN="), LoginStr);
	}

	FParse::Value(FCommandLine::Get(), TEXT("PASSWORD="), PasswordStr);
	if (!PasswordStr.IsEmpty())
	{
		UE_LOG_ONLINE(Warning, TEXT("PASSWORD commandline parameter is deprecated, use AUTH_PASSWORD"));
	}
	else
	{
		FParse::Value(FCommandLine::Get(), TEXT("AUTH_PASSWORD="), PasswordStr);
	}

	FParse::Value(FCommandLine::Get(), TEXT("TYPE="), TypeStr);
	if (!TypeStr.IsEmpty())
	{
		UE_LOG_ONLINE(Warning, TEXT("TYPE commandline parameter is deprecated, use AUTH_TYPE"));
	}
	else
	{
		FParse::Value(FCommandLine::Get(), TEXT("AUTH_TYPE="), TypeStr);
	}
	
	if (!LoginStr.IsEmpty())
	{
		if (!PasswordStr.IsEmpty())
		{
			if (!TypeStr.IsEmpty())
			{
				return Login(0, FOnlineAccountCredentials(TypeStr, LoginStr, PasswordStr));
			}
			else
			{
				UE_LOG_ONLINE(Warning, TEXT("AutoLogin missing TYPE=<type>."));
			}
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("AutoLogin missing PASSWORD=<password>."));
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("AutoLogin missing LOGIN=<login id>."));
	}
	return false;
}

TSharedPtr<FUserOnlineAccount> FOnlineIdentityNull::GetUserAccount(const FUniqueNetId& UserId) const
{
	TSharedPtr<FUserOnlineAccount> Result;

	FUniqueNetIdString StringUserId(UserId);
	const TSharedRef<FUserOnlineAccountNull>* FoundUserAccount = UserAccounts.Find(StringUserId);
	if (FoundUserAccount != NULL)
	{
		Result = *FoundUserAccount;
	}

	return Result;
}

TArray<TSharedPtr<FUserOnlineAccount> > FOnlineIdentityNull::GetAllUserAccounts() const
{
	TArray<TSharedPtr<FUserOnlineAccount> > Result;
	
	for (TMap<FUniqueNetIdString, TSharedRef<FUserOnlineAccountNull>>::TConstIterator It(UserAccounts); It; ++It)
	{
		Result.Add(It.Value());
	}

	return Result;
}

TSharedPtr<FUniqueNetId> FOnlineIdentityNull::GetUniquePlayerId(int32 LocalUserNum) const
{
	const TSharedPtr<FUniqueNetId>* FoundId = UserIds.Find(LocalUserNum);
	if (FoundId != NULL)
	{
		return *FoundId;
	}
	return NULL;
}

TSharedPtr<FUniqueNetId> FOnlineIdentityNull::CreateUniquePlayerId(uint8* Bytes, int32 Size)
{
	if (Bytes != NULL && Size > 0)
	{
		FString StrId(Size, (TCHAR*)Bytes);
		return MakeShareable(new FUniqueNetIdString(StrId));
	}
	return NULL;
}

TSharedPtr<FUniqueNetId> FOnlineIdentityNull::CreateUniquePlayerId(const FString& Str)
{
	return MakeShareable(new FUniqueNetIdString(Str));
}

ELoginStatus::Type FOnlineIdentityNull::GetLoginStatus(int32 LocalUserNum) const
{
	TSharedPtr<FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
	if (UserId.IsValid())
	{
		return GetLoginStatus(*UserId);
	}
	return ELoginStatus::NotLoggedIn;
}

ELoginStatus::Type FOnlineIdentityNull::GetLoginStatus(const FUniqueNetId& UserId) const 
{
	TSharedPtr<FUserOnlineAccount> UserAccount = GetUserAccount(UserId);
	if (UserAccount.IsValid() &&
		UserAccount->GetUserId()->IsValid())
	{
		return ELoginStatus::LoggedIn;
	}
	return ELoginStatus::NotLoggedIn;
}

FString FOnlineIdentityNull::GetPlayerNickname(int32 LocalUserNum) const
{
	TSharedPtr<FUniqueNetId> UniqueId = GetUniquePlayerId(LocalUserNum);
	if (UniqueId.IsValid())
	{
		return UniqueId->ToString();
	}

	return TEXT("NullUser");
}

FString FOnlineIdentityNull::GetPlayerNickname(const FUniqueNetId& UserId) const
{
	return UserId.ToString();
}

FString FOnlineIdentityNull::GetAuthToken(int32 LocalUserNum) const
{
	TSharedPtr<FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
	if (UserId.IsValid())
	{
		TSharedPtr<FUserOnlineAccount> UserAccount = GetUserAccount(*UserId);
		if (UserAccount.IsValid())
		{
			return UserAccount->GetAccessToken();
		}
	}
	return FString();
}

FOnlineIdentityNull::FOnlineIdentityNull(class FOnlineSubsystemNull* InSubsystem)
{
	// autologin the 0-th player
	Login(0, FOnlineAccountCredentials(TEXT("DummyType"), TEXT("DummyUser"), TEXT("DummyId")) );
}

FOnlineIdentityNull::FOnlineIdentityNull()
{
}

FOnlineIdentityNull::~FOnlineIdentityNull()
{
}

void FOnlineIdentityNull::GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate)
{
	Delegate.ExecuteIfBound(UserId, Privilege, (uint32)EPrivilegeResults::NoFailures);
}

FPlatformUserId FOnlineIdentityNull::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId)
{
	for (int i = 0; i < MAX_LOCAL_PLAYERS; ++i)
	{
		auto CurrentUniqueId = GetUniquePlayerId(i);
		if (CurrentUniqueId.IsValid() && (*CurrentUniqueId == UniqueNetId))
		{
			return i;
		}
	}

	return PLATFORMUSERID_NONE;
}

