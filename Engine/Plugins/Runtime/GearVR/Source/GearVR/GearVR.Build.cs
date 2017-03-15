// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class GearVR : ModuleRules
	{
		public GearVR(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(new string[]
				{
					"GearVR/Private",
					"../../../../Source/Runtime/Renderer/Private",
					"../../../../Source/Runtime/Launch/Private",
					"../../../../Source/ThirdParty/Oculus/Common",
					"../../../../Source/Runtime/OpenGLDrv/Private",
				});

			PrivateDependencyModuleNames.AddRange(new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"InputCore",
					"RHI",
					"RenderCore",
					"Renderer",
					"ShaderCore",
					"HeadMountedDisplay",
					"OculusMobile",
					"UtilityShaders",
				});

			if (UEBuildConfiguration.bBuildEditor)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
			}

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, BuildConfiguration.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add(new ReceiptProperty("AndroidPlugin", Path.Combine(PluginPath, "GearVR_APL.xml")));
			}

			if (Target.Platform != UnrealTargetPlatform.Mac)
			{
				PrivateDependencyModuleNames.Add("OpenGLDrv");
			}

			PublicIncludePathModuleNames.Add("Launch");

			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");
		}
	}
}
