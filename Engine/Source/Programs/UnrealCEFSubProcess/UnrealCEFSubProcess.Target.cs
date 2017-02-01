// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using UnrealBuildTool;

[SupportedPlatforms(UnrealTargetPlatform.Win32, UnrealTargetPlatform.Win64, UnrealTargetPlatform.Mac, UnrealTargetPlatform.Linux)]
public class UnrealCEFSubProcessTarget : TargetRules
{
	public UnrealCEFSubProcessTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		LaunchModuleName = "UnrealCEFSubProcess";
	}

	//
	// TargetRules interface.
	//

	public override void SetupGlobalEnvironment(
		TargetInfo Target,
		ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
		ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
		)
	{
		// Turn off various third party features we don't need

		// Currently we force Lean and Mean mode
		UEBuildConfiguration.bCompileLeanAndMeanUE = true;

		// Currently this app is not linking against the engine, so we'll compile out references from Core to the rest of the engine
		UEBuildConfiguration.bCompileAgainstEngine = false;
		UEBuildConfiguration.bCompileAgainstCoreUObject = false;
		UEBuildConfiguration.bBuildWithEditorOnlyData = true;

		// Never use malloc profiling in CEFSubProcess.
		BuildConfiguration.bUseMallocProfiler = false;

		// Force all shader formats to be built and included.
		//UEBuildConfiguration.bForceBuildShaderFormats = true;

		// CEFSubProcess is a console application, not a Windows app (sets entry point to main(), instead of WinMain())
		OutLinkEnvironmentConfiguration.bIsBuildingConsoleApplication = false;

		// Disable logging, as the sub processes are spawned often and logging will just slow them down
		OutCPPEnvironmentConfiguration.Definitions.Add("ALLOW_LOG_FILE=0");

		// Epic Games Launcher needs to run on OS X 10.9, so CEFSubProcess needs this as well
		OutCPPEnvironmentConfiguration.bEnableOSX109Support = true;
	}
}
