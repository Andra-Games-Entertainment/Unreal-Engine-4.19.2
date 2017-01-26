// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "IMovieScenePlayer.h"
#include "Evaluation/MovieScenePlayback.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "MovieSceneSequencePlayer.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.generated.h"

class AActor;
class FLevelSequenceSpawnRegister;
class FViewportClient;
class UCameraComponent;

struct DEPRECATED(4.15, "Please use FMovieSceneSequencePlaybackSettings.") FLevelSequencePlaybackSettings
	: public FMovieSceneSequencePlaybackSettings
{};

USTRUCT(BlueprintType)
struct FLevelSequenceSnapshotSettings
{
	GENERATED_BODY()

	FLevelSequenceSnapshotSettings()
		: ZeroPadAmount(4), FrameRate(30)
	{}

	FLevelSequenceSnapshotSettings(int32 InZeroPadAmount, float InFrameRate)
		: ZeroPadAmount(InZeroPadAmount), FrameRate(InFrameRate)
	{}

	/** Zero pad frames */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	uint8 ZeroPadAmount;

	/** Playback framerate */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	float FrameRate;
};

/**
 * Frame snapshot information for a level sequence
 */
USTRUCT(BlueprintType)
struct FLevelSequencePlayerSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FText MasterName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	float MasterTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FText CurrentShotName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	float CurrentShotLocalTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	UCameraComponent* CameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FLevelSequenceSnapshotSettings Settings;
};

/**
 * ULevelSequencePlayer is used to actually "play" an level sequence asset at runtime.
 *
 * This class keeps track of playback state and provides functions for manipulating
 * an level sequence while its playing.
 */
UCLASS(BlueprintType)
class LEVELSEQUENCE_API ULevelSequencePlayer
	: public UMovieSceneSequencePlayer
{
public:
	ULevelSequencePlayer(const FObjectInitializer&);

	GENERATED_BODY()

	/**
	 * Initialize the player.
	 *
	 * @param InLevelSequence The level sequence to play.
	 * @param InWorld The world that the animation is played in.
	 * @param Settings The desired playback settings
	 */
	void Initialize(ULevelSequence* InLevelSequence, UWorld* InWorld, const FMovieSceneSequencePlaybackSettings& Settings);

public:

	/**
	 * Create a new level sequence player.
	 *
	 * @param WorldContextObject Context object from which to retrieve a UWorld.
	 * @param LevelSequence The level sequence to play.
	 * @param Settings The desired playback settings
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", meta=(WorldContext="WorldContextObject"))
	static ULevelSequencePlayer* CreateLevelSequencePlayer(UObject* WorldContextObject, ULevelSequence* LevelSequence, FMovieSceneSequencePlaybackSettings Settings);

	/** Set the settings used to capture snapshots with */
	void SetSnapshotSettings(const FLevelSequenceSnapshotSettings& InSettings) { SnapshotSettings = InSettings; }

public:

	/**
	 * Access the level sequence this player is playing
	 * @return the level sequence currently assigned to this player
	 */
	DEPRECATED(4.15, "Please use GetSequence instead.")
	ULevelSequence* GetLevelSequence() const { return Cast<ULevelSequence>(Sequence); }

protected:

	// IMovieScenePlayer interface
	virtual void UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject, bool bJumpCut) override;
	virtual UObject* GetPlaybackContext() const override;
	virtual TArray<UObject*> GetEventContexts() const override;

	//~ UMovieSceneSequencePlayer interface
	virtual bool CanPlay() const override;
	virtual void OnStartedPlaying() override;
	virtual void OnStopped() override;

public:

	/** Populate the specified array with any given event contexts for the specified world */
	static void GetEventContexts(UWorld& InWorld, TArray<UObject*>& OutContexts);

	/** Take a snapshot of the current state of this player */
	void TakeFrameSnapshot(FLevelSequencePlayerSnapshot& OutSnapshot) const;

private:

	/** Add tick prerequisites so that the level sequence actor ticks before all the actors it controls */
	void SetTickPrerequisites(bool bAddTickPrerequisites);

	void SetTickPrerequisites(FMovieSceneSequenceID SequenceID, UMovieSceneSequence* Sequence, bool bAddTickPrerequisites);

private:

	/** The world this player will spawn actors in, if needed */
	TWeakObjectPtr<UWorld> World;

	/** The last view target to reset to when updating camera cuts to null */
	TWeakObjectPtr<AActor> LastViewTarget;

protected:

	/** How to take snapshots */
	FLevelSequenceSnapshotSettings SnapshotSettings;

	TWeakObjectPtr<UCameraComponent> CachedCameraComponent;
};
