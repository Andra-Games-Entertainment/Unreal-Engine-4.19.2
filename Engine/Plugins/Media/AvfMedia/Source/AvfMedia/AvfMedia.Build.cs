// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AvfMedia : ModuleRules
	{
		public AvfMedia(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
                    "AvfMediaFactory",
					"Core",
					"RenderCore",
					"RHI",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"AvfMedia/Private",
					"AvfMedia/Private/Player",
					"AvfMedia/Private/Tracks",
				}
			);

			PublicFrameworks.AddRange(
				new string[] {
					"CoreMedia",
					"AVFoundation",
					"AudioToolbox",
					"QuartzCore"
				}
			);
		}
	}
}
