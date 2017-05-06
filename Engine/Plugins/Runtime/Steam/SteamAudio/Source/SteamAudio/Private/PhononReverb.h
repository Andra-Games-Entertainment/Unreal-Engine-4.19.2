//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "IAudioExtensionPlugin.h"
#include "Sound/SoundEffectSubmix.h"
#include "Sound/SoundEffectPreset.h"
#include "PhononCommon.h"
#include "phonon.h"
#include "PhononReverb.generated.h"

namespace SteamAudio
{
	struct FReverbSource
	{
		FReverbSource();

		IPLhandle ConvolutionEffect;
		float IndirectContribution;
		IPLAudioBuffer InBuffer;
		TArray<float> IndirectInArray;
	};

	class FPhononReverb : public IAudioReverb
	{
	public:
		FPhononReverb();
		~FPhononReverb();

		virtual void Initialize(const int32 SampleRate, const int32 NumSources, const int32 FrameSize) override;
		virtual void OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, UReverbPluginSourceSettingsBase* InSettings) override;
		virtual void OnReleaseSource(const uint32 SourceId) override;
		virtual class FSoundEffectSubmix* GetEffectSubmix(class USoundSubmix* Submix) override;
		virtual void ProcessSourceAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData) override;
		
		void ProcessMixedAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData);
		void SetEnvironmentalRenderer(IPLhandle EnvironmentalRenderer);
		void CreateReverbEffect();
		void UpdateListener(const FVector& Position, const FVector& Forward, const FVector& Up);

	private:
		IPLhandle EnvironmentalRenderer;
		IPLhandle BinauralRenderer;
		IPLhandle IndirectBinauralEffect;
		IPLhandle IndirectPanningEffect;
		IPLhandle ReverbConvolutionEffect;
		IPLAudioBuffer DryBuffer;

		IPLAudioBuffer IndirectOutBuffer;
		int32 AmbisonicsChannels;
		float** IndirectOutDeinterleaved;
		TArray<float> IndirectOutArray;

		IPLAudioBuffer IndirectIntermediateBuffer;
		TArray<float> IndirectIntermediateArray;

		IPLAudioFormat InputAudioFormat;
		IPLAudioFormat ReverbInputAudioFormat;
		IPLAudioFormat IndirectOutputAudioFormat;
		IPLAudioFormat BinauralOutputAudioFormat;

		FCriticalSection ListenerCriticalSection;
		IPLVector3 ListenerPosition;
		IPLVector3 ListenerForward;
		IPLVector3 ListenerUp;

		IPLRenderingSettings RenderingSettings;

		TArray<FReverbSource> ReverbSources;

		float ReverbIndirectContribution;
		TArray<float> ReverbIndirectInArray;

		class FSteamAudioModule* SteamAudioModule;
	};
}

class FSubmixEffectReverbPlugin : public FSoundEffectSubmix
{
public:
	FSubmixEffectReverbPlugin();

	virtual void Init(const FSoundEffectSubmixInitData& InSampleRate) override;
	virtual uint32 GetDesiredInputChannelCountOverride() const override;
	virtual void OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData) override;
	virtual void OnPresetChanged() override;

	void SetPhononReverbPlugin(SteamAudio::FPhononReverb* PhononReverbPlugin);

private:
	SteamAudio::FPhononReverb* PhononReverbPlugin;
};

USTRUCT()
struct FSubmixEffectReverbPluginSettings
{
	GENERATED_USTRUCT_BODY()
};

UCLASS()
class USubmixEffectReverbPluginPreset : public USoundEffectSubmixPreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS_NO_ASSET_ACTIONS(SubmixEffectReverbPlugin)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixEffectPreset)
	FSubmixEffectReverbPluginSettings Settings;
};
