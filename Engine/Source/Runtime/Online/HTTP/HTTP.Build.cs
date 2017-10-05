// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HTTP : ModuleRules
{
	public HTTP(ReadOnlyTargetRules Target) : base(Target)
	{
		Definitions.Add("HTTP_PACKAGE=1");

		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/Online/HTTP/Private",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
			}
			);

		bool bWithCurl = false;

		if (Target.Platform == UnrealTargetPlatform.Win32 ||
			Target.Platform == UnrealTargetPlatform.Win64)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "WinInet");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "WinHttp");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "libcurl");

			bWithCurl = true;

			PrivateDependencyModuleNames.Add("SSL");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux ||
			Target.Platform == UnrealTargetPlatform.Android ||
			Target.Platform == UnrealTargetPlatform.Switch)
		{
            AddEngineThirdPartyPrivateStaticDependencies(Target, "libcurl");
            PrivateDependencyModuleNames.Add("SSL");

			bWithCurl = true;
		}
		else
		{
			Definitions.Add("WITH_SSL=0");
			Definitions.Add("WITH_LIBCURL=0");
		}

		if (bWithCurl)
		{
			Definitions.Add("CURL_ENABLE_DEBUG_CALLBACK=1");
			if (Target.Configuration != UnrealTargetConfiguration.Shipping)
			{
				Definitions.Add("CURL_ENABLE_NO_TIMEOUTS_OPTION=1");
			}
		}

		if (Target.Platform == UnrealTargetPlatform.HTML5)
		{
			PrivateDependencyModuleNames.Add("HTML5JS");
		}

		if (Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.TVOS || Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicFrameworks.Add("Security");
		}
	}
}
