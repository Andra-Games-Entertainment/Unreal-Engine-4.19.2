// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE:  All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law.  Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY.  Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure  of  this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of  COMPANY.   ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC  PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE  OF THIS
// SOURCE CODE  WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES.  THE RECEIPT OR POSSESSION OF  THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT  MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------*/
// %BANNER_END%

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"
#include "Misc/Guid.h"
#include "IMediaCache.h"
#include "IMediaControls.h"
#include "IMediaPlayer.h"
#include "IMediaTracks.h"
#include "IMediaView.h"
#include "Misc/Timespan.h"
#include "Templates/SharedPointer.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "RHIResources.h"

#include "ml_api.h"

class FMediaSamples;
class IMediaEventSink;
class FMagicLeapMediaTextureSamplePool;
struct FMagicLeapVideoTextureData;

/**
 *	Implement media playback using the MagicLeap MediaPlayer interface.
 */
class FMagicLeapMediaPlayer : public IMediaPlayer, public IMediaControls, public IMediaCache, public IMediaTracks, public IMediaView
{
public:
  
	/**
	 * Create and initialize a new instance.
	 *
	 * @param InEventSink The object that receives media events from this player.
	 */
	FMagicLeapMediaPlayer(IMediaEventSink& InEventSink);

	/** Virtual destructor. */
	virtual ~FMagicLeapMediaPlayer();

public:
  /** IMediaPlayer interface */
	virtual void Close() override;
	virtual IMediaCache& GetCache() override;
	virtual IMediaControls& GetControls() override;
	virtual FString GetInfo() const override;
	virtual FName GetPlayerName() const override;
	virtual IMediaSamples& GetSamples() override;
	virtual FString GetStats() const override;
	virtual IMediaTracks& GetTracks() override;
	virtual FString GetUrl() const override;
	virtual IMediaView& GetView() override;
	virtual bool Open(const FString& Url, const IMediaOptions* Options) override;
	virtual bool Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions* Options) override;
	virtual void SetGuid(const FGuid& Guid) override;
	virtual void TickFetch(FTimespan DeltaTime, FTimespan Timecode) override;
	virtual void TickInput(FTimespan DeltaTime, FTimespan Timecode) override;

public:
	/** IMediaControls interface */
	virtual bool CanControl(EMediaControl Control) const override;
	virtual FTimespan GetDuration() const override;
	virtual float GetRate() const override;
	virtual EMediaState GetState() const override;
	virtual EMediaStatus GetStatus() const override;
	virtual TRangeSet<float> GetSupportedRates(EMediaRateThinning Thinning) const override;
	virtual FTimespan GetTime() const override;
	virtual bool IsLooping() const override;
	virtual bool Seek(const FTimespan& Time) override;
	virtual bool SetLooping(bool Looping) override;
	virtual bool SetRate(float Rate) override;
	virtual bool SetNativeVolume(float Volume) override;

public:
	/** IMediaTracks interface */
	virtual bool GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const override;
	virtual int32 GetNumTracks(EMediaTrackType TrackType) const override;
	virtual int32 GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual int32 GetSelectedTrack(EMediaTrackType TrackType) const override;
	virtual FText GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual int32 GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual FString GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual FString GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual bool GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const override;
	virtual bool SelectTrack(EMediaTrackType TrackType, int32 TrackIndex) override;
	virtual bool SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex) override;

private:
	bool GetMediaPlayerState(uint16 FlagToPoll) const;

	/** Callback for when the application resumed in the foreground. */
	void HandleApplicationHasEnteredForeground();

	/** Callback for when the application is being paused in the background. */
	void HandleApplicationWillEnterBackground();

private:

	// TODO: mature this to return track dimensions.
	FIntPoint GetVideoDimensions() const;

	MLHandle MediaPlayerHandle;

	bool bMediaPrepared;

	/** Current player state. */
	EMediaState CurrentState;

	/** Current state of looping. */
	bool bLooping;

	/** The media event handler. */
	IMediaEventSink& EventSink;

	/** Media information string. */
	FString Info;

	/** Currently opened media. */
	FString MediaUrl;

	/** Media player Guid */
	FGuid PlayerGuid;

	/** Foreground/background delegate for pause. */
	FDelegateHandle PauseHandle;

	/** Foreground/background delegate for resume. */
	FDelegateHandle ResumeHandle;

	/** The media sample queue. */
	TSharedPtr<FMediaSamples, ESPMode::ThreadSafe> Samples;

	/** Video sample object pool. */
	FMagicLeapMediaTextureSamplePool* VideoSamplePool;

	TSharedPtr<FMagicLeapVideoTextureData, ESPMode::ThreadSafe> TextureData;

	TMap<EMediaTrackType, TArray<int32>> TrackInfo;

	TMap<EMediaTrackType, int32> SelectedTrack;

	class FMediaWorker* MediaWorker;

	FCriticalSection CriticalSection;

	bool bWasMediaPlayingBeforeAppPause;
};
