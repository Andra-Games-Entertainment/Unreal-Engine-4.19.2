// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MagicLeapAudio : ModuleRules
{
	public MagicLeapAudio(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("TargetPlatform");

		//Todo: implement this in a more portable way.
		string EngineSourceDirectory = "../../../../Source";
		PublicIncludePaths.Add(Path.Combine(EngineSourceDirectory, "Runtime/AudioMixer/Public"));
		PrivateIncludePaths.Add(Path.Combine(EngineSourceDirectory, "Runtime/AudioMixer/Private"));

		PrivateIncludePaths.Add("MagicLeap/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"MLSDK",
				"AudioMixer"
			}
		);

		AddEngineThirdPartyPrivateStaticDependencies(Target,
			"UEOgg",
			"Vorbis",
			"VorbisFile"
		);
	}
}
