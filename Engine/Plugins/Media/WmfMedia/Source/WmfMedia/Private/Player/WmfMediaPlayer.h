// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../WmfMediaPrivate.h"
#include "WmfMediaPlayer.h"
#include "IMediaPlayer.h"
#include "IMediaTracks.h"
#include "IWmfMediaResolverCallbacks.h"
#include "WmfMediaTracks.h"
#include "Containers/Ticker.h"
#include "Containers/Queue.h"

#if WMFMEDIA_SUPPORTED_PLATFORM

#include "AllowWindowsPlatformTypes.h"

class FWmfMediaResolver;
class FWmfMediaSession;
class IMediaControls;
class IMediaOutput;
class IMediaTracks;


/**
 * Implements a media player using the Windows Media Foundation framework.
 */
class FWmfMediaPlayer
	: public IMediaPlayer
	, protected IWmfMediaResolverCallbacks
{
public:

	/** Default constructor. */
	FWmfMediaPlayer();

	/** Destructor. */
	~FWmfMediaPlayer();

public:

	//~ IMediaPlayer interface

	virtual void Close() override;
	virtual IMediaControls& GetControls() override;
	virtual FString GetInfo() const override;
	virtual FName GetName() const override;
	virtual IMediaOutput& GetOutput() override;
	virtual FString GetStats() const override;
	virtual IMediaTracks& GetTracks() override;
	virtual FString GetUrl() const override;
	virtual bool Open(const FString& Url, const IMediaOptions& Options) override;
	virtual bool Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions& Options) override;
	virtual void TickPlayer(float DeltaTime) override;
	virtual void TickVideo(float DeltaTime) override;
	
	DECLARE_DERIVED_EVENT(FWmfMediaPlayer, IMediaPlayer::FOnMediaEvent, FOnMediaEvent);
	virtual FOnMediaEvent& OnMediaEvent() override
	{
		return MediaEvent;
	}

protected:

	//~ IWmfMediaResolverCallbacks interface

	virtual void ProcessResolveComplete(TComPtr<IUnknown> SourceObject, FString ResolvedUrl) override;
	virtual void ProcessResolveFailed(FString FailedUrl) override;

protected:

	/**
	 * Initializes the media session for the given media source.
	 *
	 * @param SourceObject The media source object.
	 * @param SourceUrl The original URL of the media source.
	 * @return true on success, false otherwise.
	 */
	bool InitializeMediaSession(IUnknown* SourceObject, const FString& SourceUrl);

private:

	/** Handles session events. */
	void HandleSessionEvent(MediaEventType EventType);

private:

	/** The duration of the currently loaded media. */
	FTimespan Duration;

	/** Media information string. */
	FString Info;

	/** Tasks to execute on the player thread. */
	TQueue<TFunction<void()>> PlayerTasks;

	/** Event delegate that is invoked when a media event occurred. */
	FOnMediaEvent MediaEvent;

	/** Asynchronous callback object for the media stream. */
	TComPtr<FWmfMediaSession> MediaSession;

	/** Pointer to the media source object. */
	TComPtr<IMFMediaSource> MediaSource;

	/** The URL of the currently opened media. */
	FString MediaUrl;

	/** The media source resolver. */
	TComPtr<FWmfMediaResolver> Resolver;

	/** Track manager. */
	FWmfMediaTracks Tracks;
};


#include "HideWindowsPlatformTypes.h"

#endif