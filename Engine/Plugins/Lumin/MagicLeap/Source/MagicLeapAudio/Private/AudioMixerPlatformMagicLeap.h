// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMixer.h"
#include <ml_audio.h>

// Any platform defines
namespace Audio
{

	class FMixerPlatformMagicLeap : public IAudioMixerPlatformInterface
	{

	public:

		FMixerPlatformMagicLeap();
		~FMixerPlatformMagicLeap();

		//~ Begin IAudioMixerPlatformInterface
		virtual EAudioMixerPlatformApi::Type GetPlatformApi() const override { return EAudioMixerPlatformApi::Null; }
		virtual bool InitializeHardware() override;
		virtual bool TeardownHardware() override;
		virtual bool IsInitialized() const override;
		virtual bool GetNumOutputDevices(uint32& OutNumOutputDevices) override;
		virtual bool GetOutputDeviceInfo(const uint32 InDeviceIndex, FAudioPlatformDeviceInfo& OutInfo) override;
		virtual bool GetDefaultOutputDeviceIndex(uint32& OutDefaultDeviceIndex) const override;
		virtual bool OpenAudioStream(const FAudioMixerOpenStreamParams& Params) override;
		virtual bool CloseAudioStream() override;
		virtual bool StartAudioStream() override;
		virtual bool StopAudioStream() override;
		virtual FAudioPlatformDeviceInfo GetPlatformDeviceInfo() const override;
		virtual void SubmitBuffer(const uint8* Buffer) override;
		virtual FName GetRuntimeFormat(USoundWave* InSoundWave) override;
		virtual bool HasCompressedAudioInfoClass(USoundWave* InSoundWave) override;
		virtual ICompressedAudioInfo* CreateCompressedAudioInfo(USoundWave* InSoundWave) override;
		virtual FString GetDefaultDeviceName() override;
		virtual FAudioPlatformSettings GetPlatformSettings() const override;
		virtual void SuspendContext() override;
		virtual void ResumeContext() override;

		//~ End IAudioMixerPlatformInterface

		uint8* CachedBufferHandle;


		virtual int32 GetNumFrames(const int32 InNumReqestedFrames) override;

	private:
		static const TCHAR* GetErrorString(MLAudioError Result);

		bool bSuspended;
		bool bInitialized;
		bool bInCallback;

		MLHandle StreamHandle;

		// Static callback used for MLAudio:
		static void MLAudioCallback(void* CallbackContext);
	};

}

