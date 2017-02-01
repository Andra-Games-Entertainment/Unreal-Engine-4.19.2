// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameProjectGeneration : ModuleRules
{
    public GameProjectGeneration(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePaths.AddRange(new string[] { "GameProjectGeneration/Private", "GameProjectGeneration/Public", "GameProjectGeneration/Classes" });

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"HardwareTargeting",
			}
		);

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
				"AssetRegistry",
				"ContentBrowser",
                "DesktopPlatform",
                "MainFrame",
				"AddContentDialog",
				"HardwareTargeting",
            }
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Analytics",
                "AppFramework",
				"ClassViewer",
				"Core",
				"CoreUObject",
				"Engine",
				"EngineSettings",
                "InputCore",
				"Projects",
                "RenderCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
                "SourceControl",
 				"TargetPlatform",
				"UnrealEd",
				"DesktopPlatform",
                "HardwareTargeting",
				"AddContentDialog",
			}
		);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
				"AssetRegistry",
				"ContentBrowser",
                "Documentation",
                "MainFrame",
            }
		);
	}
}
