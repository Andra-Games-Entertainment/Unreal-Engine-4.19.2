// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/NetworkVersion.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Misc/NetworkGuid.h"

DEFINE_LOG_CATEGORY( LogNetVersion );

FNetworkVersion::FGetLocalNetworkVersionOverride FNetworkVersion::GetLocalNetworkVersionOverride;
FNetworkVersion::FIsNetworkCompatibleOverride FNetworkVersion::IsNetworkCompatibleOverride;

FString FNetworkVersion::ProjectVersion;

enum EEngineNetworkVersionHistory
{
	HISTORY_INITIAL					= 1,
	HISTORY_REPLAY_BACKWARDS_COMPAT	= 2,		// Bump version to get rid of older replays before backwards compat was turned on officially
};

bool FNetworkVersion::bHasCachedNetworkChecksum			= false;
uint32 FNetworkVersion::CachedNetworkChecksum			= 0;

uint32 FNetworkVersion::EngineNetworkProtocolVersion	= HISTORY_REPLAY_BACKWARDS_COMPAT;
uint32 FNetworkVersion::GameNetworkProtocolVersion		= 0;

uint32 FNetworkVersion::EngineCompatibleNetworkProtocolVersion		= HISTORY_REPLAY_BACKWARDS_COMPAT;
uint32 FNetworkVersion::GameCompatibleNetworkProtocolVersion		= 0;

uint32 FNetworkVersion::GetNetworkCompatibleChangelist()
{
	//return FEngineVersion::Current().GetChangelist();
	return ENGINE_NET_VERSION;
}

uint32 FNetworkVersion::GetReplayCompatibleChangelist()
{
	return BUILT_FROM_CHANGELIST;
}

uint32 FNetworkVersion::GetEngineNetworkProtocolVersion()
{
	return EngineNetworkProtocolVersion;
}

uint32 FNetworkVersion::GetEngineCompatibleNetworkProtocolVersion()
{
	return EngineCompatibleNetworkProtocolVersion;
}

uint32 FNetworkVersion::GetGameNetworkProtocolVersion()
{
	return GameNetworkProtocolVersion;
}

uint32 FNetworkVersion::GetGameCompatibleNetworkProtocolVersion()
{
	return GameCompatibleNetworkProtocolVersion;
}

uint32 FNetworkVersion::GetLocalNetworkVersion( bool AllowOverrideDelegate /*=true*/ )
{
	if ( bHasCachedNetworkChecksum )
	{
		return CachedNetworkChecksum;
	}

	if ( AllowOverrideDelegate && GetLocalNetworkVersionOverride.IsBound() )
	{
		CachedNetworkChecksum = GetLocalNetworkVersionOverride.Execute();

		UE_LOG( LogNetVersion, Log, TEXT( "Checksum from delegate: %u" ), CachedNetworkChecksum );

		bHasCachedNetworkChecksum = true;

		return CachedNetworkChecksum;
	}

	FString VersionString = FString::Printf(TEXT("%s %s, NetCL: %d, EngineNetVer: %d, GameNetVer: %d"),
		FApp::GetGameName(),
		*ProjectVersion,
		GetNetworkCompatibleChangelist(),
		FNetworkVersion::GetEngineNetworkProtocolVersion(),
		FNetworkVersion::GetGameNetworkProtocolVersion());

	CachedNetworkChecksum = FCrc::StrCrc32(*VersionString.ToLower());

	UE_LOG(LogNetVersion, Log, TEXT("%s (Checksum: %u)"), *VersionString, CachedNetworkChecksum);

	bHasCachedNetworkChecksum = true;

	return CachedNetworkChecksum;
}

bool FNetworkVersion::IsNetworkCompatible( const uint32 LocalNetworkVersion, const uint32 RemoteNetworkVersion )
{
	if ( IsNetworkCompatibleOverride.IsBound() )
	{
		return IsNetworkCompatibleOverride.Execute( LocalNetworkVersion, RemoteNetworkVersion );
	}

	return LocalNetworkVersion == RemoteNetworkVersion;
}

FNetworkReplayVersion FNetworkVersion::GetReplayVersion()
{
	const uint32 ReplayVersion = ( GameCompatibleNetworkProtocolVersion << 16 ) | EngineCompatibleNetworkProtocolVersion;

	return FNetworkReplayVersion( FApp::GetGameName(), ReplayVersion, GetReplayCompatibleChangelist() );
}
