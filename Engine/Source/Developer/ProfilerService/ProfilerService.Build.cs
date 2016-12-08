// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ProfilerService : ModuleRules
	{
		public ProfilerService(TargetInfo Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
					"Engine",
					"ProfilerMessages",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Messaging",
				}
			);
		}
	}
}