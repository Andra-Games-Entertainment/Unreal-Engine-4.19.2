// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MixedRealityFramework : ModuleRules
{
	public MixedRealityFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange(
			new string[] {
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"MixedRealityFramework/Private"
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"MediaAssets"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Media",
				"HeadMountedDisplay",
				"InputCore",
                "MediaUtils",
				"RenderCore",
                "OpenCVHelper",
                "OpenCV"
			}
		);

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
