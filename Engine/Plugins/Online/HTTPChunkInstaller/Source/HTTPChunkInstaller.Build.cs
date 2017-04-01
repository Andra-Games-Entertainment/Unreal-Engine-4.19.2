// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HTTPChunkInstaller : ModuleRules
{
	public HTTPChunkInstaller(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"BuildPatchServices",
				"Core",
				"Engine",
				"Http",
				"Json",
				"PakFile",
			}
			);
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"OnlineSubsystem",
				"OnlineSubsystemUtils"
			}
			);

		PrivateIncludePaths.Add("../../../../../Engine/Source/Runtime/Online/BuildPatchServices/Private");

		//if (Target.Platform != UnrealTargetPlatform.Win64 && Target.Platform != UnrealTargetPlatform.Win32 && Target.Platform != UnrealTargetPlatform.IOS)
		{
			PrecompileForTargets = PrecompileTargetsType.None;
		}

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PrivateDependencyModuleNames.Add("OpenGLDrv");
			AddEngineThirdPartyPrivateStaticDependencies(Target, new string[] { "OpenGL" });
			PrivateIncludePaths.AddRange(
				new string[] {
					"../../../../../Engine/Source/Runtime/OpenGLDrv/Private",
					// ... add other private include paths required here ...
				}
			);
		}

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			// add the SystemConfiguration framework
			PublicFrameworks.AddRange(
				new string[] {
					"SystemConfiguration",
				});
		}
	}
}
