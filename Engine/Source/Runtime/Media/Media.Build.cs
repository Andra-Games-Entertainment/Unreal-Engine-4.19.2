// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Media : ModuleRules
	{
		public Media(TargetInfo Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
			); 

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/Media/Private",
				}
			);
		}
	}
}
