// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SynthComponents/SynthComponentGranulator.h"
#include "UObject/Package.h"

UGranularSynth::UGranularSynth(const FObjectInitializer& ObjInitializer)
	: Super(ObjInitializer)
	, SoundWaveCopy(nullptr)
	, PendingSoundWaveSet(nullptr)
	, bTransferPendingToSound(false)
	, bIsLoaded(false)
	, bRegistered(false)
{

}

UGranularSynth::~UGranularSynth()
{

}

void UGranularSynth::Init(const int32 SampleRate)
{
	NumChannels = 2;
}

void UGranularSynth::OnGenerateAudio(TArray<float>& OutAudio)
{
	const int32 NumFrames = OutAudio.Num() / NumChannels;
	GranularSynth.Generate(OutAudio, NumFrames);
}

void UGranularSynth::OnRegister()
{
	Super::OnRegister();

	if (!bRegistered)
	{
		bRegistered = true;
		SetComponentTickEnabled(true);
		RegisterComponent();

		FAudioDevice* AudioDevice = GetAudioDevice();
		if (AudioDevice)
		{
			GranularSynth.Init(AudioDevice, 500);

			if (SoundWave)
			{
				SoundWaveCopy = DuplicateObject<USoundWave>(SoundWave, GetTransientPackage());
				SoundWaveCopy->AddToRoot();

				GranularSynth.LoadSoundWave(SoundWaveCopy);
			}
		}
	}
}

void UGranularSynth::OnUnregister()
{
	Super::OnUnregister();

	if (IsValid(SoundWaveCopy))
	{
		SoundWaveCopy->RemoveFromRoot();
		SoundWaveCopy = nullptr;
	}
}

void UGranularSynth::SetAttackTime(const float AttackTimeMsec)
{
	SynthCommand([this, AttackTimeMsec]()
	{
		GranularSynth.SetAttackTime(AttackTimeMsec);
	});
}

void UGranularSynth::SetDecayTime(const float DecayTimeMsec)
{
	SynthCommand([this, DecayTimeMsec]()
	{
		GranularSynth.SetAttackTime(DecayTimeMsec);
	});
}

void UGranularSynth::SetSustainGain(const float SustainGain)
{
	SynthCommand([this, SustainGain]()
	{
		GranularSynth.SetSustainGain(SustainGain);
	});
}

void UGranularSynth::SetReleaseTimeMsec(const float ReleaseTimeMsec)
{
	SynthCommand([this, ReleaseTimeMsec]()
	{
		GranularSynth.SetReleaseTime(ReleaseTimeMsec);
	});
}

void UGranularSynth::NoteOn(const float Note, const int32 Velocity, const float Duration)
{
	SynthCommand([this, Note, Velocity, Duration]()
	{
		GranularSynth.NoteOn(Note, Velocity, Duration);
	});
}

void UGranularSynth::NoteOff(const float Note, const bool bKill)
{
	SynthCommand([this, Note, bKill]()
	{
		GranularSynth.NoteOff(Note, bKill);
	});
}


void UGranularSynth::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	if (bTransferPendingToSound)
	{
		bTransferPendingToSound = false;
		check(PendingSoundWaveSet);
		if (SoundWaveCopy)
		{
			SoundWaveCopy->RemoveFromRoot();
		}
		SoundWaveCopy = PendingSoundWaveSet;
		PendingSoundWaveSet = nullptr;
	}
}

void UGranularSynth::SetSoundWave(USoundWave* InSoundWave)
{
	PendingSoundWaveSet = DuplicateObject<USoundWave>(InSoundWave, GetTransientPackage());
	PendingSoundWaveSet->AddToRoot();

	SynthCommand([this]()
	{	
		GranularSynth.LoadSoundWave(PendingSoundWaveSet);
		bTransferPendingToSound = true;
	});
}

void UGranularSynth::SetGrainsPerSecond(const float GrainsPerSecond)
{
	SynthCommand([this, GrainsPerSecond]()
	{
		GranularSynth.SetGrainsPerSecond(GrainsPerSecond);
	});
}

void UGranularSynth::SetGrainProbability(const float InGrainProbability)
{
	SynthCommand([this, InGrainProbability]()
	{
		GranularSynth.SetGrainProbability(InGrainProbability);
	});
}

void UGranularSynth::SetGrainEnvelopeType(const EGranularSynthEnvelopeType EnvelopeType)
{
	SynthCommand([this, EnvelopeType]()
	{
		GranularSynth.SetGrainEnvelopeType((Audio::EGrainEnvelopeType)EnvelopeType);
	});		
}

void UGranularSynth::SetPlaybackSpeed(const float InPlayheadRate)
{
	SynthCommand([this, InPlayheadRate]()
	{
		GranularSynth.SetPlaybackSpeed(InPlayheadRate);
	});
}

void UGranularSynth::SetGrainPitch(const float BasePitch, const FVector2D PitchRange)
{
	SynthCommand([this, BasePitch, PitchRange]()
	{
		GranularSynth.SetGrainPitch(BasePitch, PitchRange);
	});
}

void UGranularSynth::SetGrainVolume(const float BaseVolume, FVector2D VolumeRange)
{
	SynthCommand([this, BaseVolume, VolumeRange]()
	{
		GranularSynth.SetGrainVolume(BaseVolume, VolumeRange);
	});
}

void UGranularSynth::SetGrainPan(const float BasePan, FVector2D PanRange)
{
	SynthCommand([this, BasePan, PanRange]()
	{
		GranularSynth.SetGrainPan(BasePan, PanRange);
	});
}

void UGranularSynth::SetGrainDuration(const float BaseDurationMsec, FVector2D DurationRange)
{
	SynthCommand([this, BaseDurationMsec, DurationRange]()
	{
		GranularSynth.SetGrainDuration(BaseDurationMsec, DurationRange);
	});
}

float UGranularSynth::GetSampleDuration() const
{
	return GranularSynth.GetSampleDuration();
}

void UGranularSynth::SetScrubMode(const bool bScrubMode)
{
	SynthCommand([this, bScrubMode]()
	{
		GranularSynth.SetScrubMode(bScrubMode);
	});
}

void UGranularSynth::SetPlayheadTime(const float InPositionSec, const float InLerpTimeSec, EGranularSynthSeekType SeekType)
{
	SynthCommand([this, InPositionSec, InLerpTimeSec, SeekType]()
	{
		GranularSynth.SeekTime(InPositionSec, InLerpTimeSec, (Audio::ESeekType::Type)SeekType);
	});
}


float UGranularSynth::GetCurrentPlayheadTime() const
{
	return GranularSynth.GetCurrentPlayheadTime();
}

bool UGranularSynth::IsLoaded() const
{
	return GranularSynth.IsSoundWaveLoaded();
}