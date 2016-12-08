// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WindowsPlatformHttp.h"
#include "HttpWinInet.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ConfigCacheIni.h"
#include "Curl/CurlHttp.h"
#include "Curl/CurlHttpManager.h"

bool bUseCurl = true;

void FWindowsPlatformHttp::Init()
{
	if (GConfig)
	{
		bool bUseCurlConfigValue = false;
		if (GConfig->GetBool(TEXT("Networking"), TEXT("UseLibCurl"), bUseCurlConfigValue, GEngineIni))
		{
			bUseCurl = bUseCurlConfigValue;
		}
	}

	// allow override on command line
	FString HttpMode;
	if (FParse::Value(FCommandLine::Get(), TEXT("HTTP="), HttpMode) &&
		(HttpMode.Equals(TEXT("WinInet"), ESearchCase::IgnoreCase)))
	{
		bUseCurl = false;
	}

#if WITH_LIBCURL
	if (bUseCurl)
	{
		FCurlHttpManager::InitCurl();
	}
#endif
}

void FWindowsPlatformHttp::Shutdown()
{
#if WITH_LIBCURL
	if (bUseCurl)
	{
		FCurlHttpManager::ShutdownCurl();
	}
	else
#endif
	{
		FWinInetConnection::Get().ShutdownConnection();
	}
}

FHttpManager * FWindowsPlatformHttp::CreatePlatformHttpManager()
{
#if WITH_LIBCURL
	if (bUseCurl)
	{
		return new FCurlHttpManager();
	}
#endif
	// allow default to be used
	return NULL;
}

IHttpRequest* FWindowsPlatformHttp::ConstructRequest()
{
#if WITH_LIBCURL
	if (bUseCurl)
	{
		return new FCurlHttpRequest();
	}
	else
#endif
	{
		return new FHttpRequestWinInet();
	}
}