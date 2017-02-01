// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LightPropagationVolumeRuntime : ModuleRules
{
	public LightPropagationVolumeRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {	
				"Core",
				"CoreUObject",
				"Engine",           // FBlendableManager
//				"RHI",
//				"RenderCore",
//				"ShaderCore",
			}
		);
	}
}
