// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "IMediaOptions.h"

#include "MediaSource.generated.h"


/**
 * Abstract base class for media sources.
 *
 * Media sources describe the location and/or settings of media objects that can
 * be played in a media player, such as a video file on disk, a video stream on
 * the internet, or a web cam attached to or built into the target device.
 */
UCLASS(Abstract, BlueprintType, hidecategories=(Object))
class MEDIAASSETS_API UMediaSource
	: public UObject
	, public IMediaOptions
{
	GENERATED_BODY()

public:

	/** Get the media source's URL string (must override in child classes). */
	virtual FString GetUrl() const PURE_VIRTUAL(UMediaSource::GetUrl, return FString(););

	/** Validate the media source settings (must override in child classes). */
	virtual bool Validate() const PURE_VIRTUAL(UMediaSource::Validate, return false;);

public:

	//~ IMediaOptions interface

	virtual FName GetDesiredPlayerName() const override;
	virtual bool GetMediaOption(const FName& Key, bool DefaultValue) const override;
	virtual double GetMediaOption(const FName& Key, double DefaultValue) const override;
	virtual int64 GetMediaOption(const FName& Key, int64 DefaultValue) const override;
	virtual FString GetMediaOption(const FName& Key, const FString& DefaultValue) const override;
	virtual FText GetMediaOption(const FName& Key, const FText& DefaultValue) const override;
	virtual bool HasMediaOption(const FName& Key) const override;
};
