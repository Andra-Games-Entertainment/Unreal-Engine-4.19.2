// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

//@todo Sequencer - these have to be here for now because this class contains a small amount of implementation inside this header to avoid exporting this class

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "ISequencer.h"
#include "Framework/Commands/UICommandList.h"
#include "ScopedTransaction.h"
#include "MovieSceneTrack.h"
#include "ISequencerTrackEditor.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"

class FMenuBuilder;
class FPaintArgs;
class FSlateWindowElementList;
class SHorizontalBox;

/** Delegate for adding keys for a property
 * float - The time at which to add the key.
 * return - True if any data was changed as a result of the call, otherwise false.
 */
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnKeyProperty, float)

DECLARE_DELEGATE_RetVal_OneParam(bool, FCanKeyProperty, float)


/**
 * Base class for handling key and section drawing and manipulation
 * of a UMovieSceneTrack class.
 *
 * @todo Sequencer Interface needs cleanup
 */
class FMovieSceneTrackEditor
	: public TSharedFromThis<FMovieSceneTrackEditor>
	, public ISequencerTrackEditor
{
public:

	/** Constructor */
	FMovieSceneTrackEditor(TSharedRef<ISequencer> InSequencer)
		: Sequencer(InSequencer)
	{ }

	/** Destructor */
	virtual ~FMovieSceneTrackEditor() { };

public:

	/** @return The current movie scene */
	UMovieSceneSequence* GetMovieSceneSequence() const
	{
		return Sequencer.Pin()->GetFocusedMovieSceneSequence();
	}

	/**
	 * @return The current local time at which we should add a key
	 */
	float GetTimeForKey()
	{ 
		TSharedPtr<ISequencer> SequencerPin = Sequencer.Pin();
		return SequencerPin.IsValid() ? SequencerPin->GetLocalTime() : 0.f;
	}

	void UpdatePlaybackRange()
	{
		TSharedPtr<ISequencer> SequencerPin = Sequencer.Pin();
		if( SequencerPin.IsValid()  )
		{
			SequencerPin->UpdatePlaybackRange();
		}
	}

/** Creates new keys after an animatable property is changed */
	SEQUENCER_API void AnimatablePropertyChanged(FOnKeyProperty OnKeyProperty);

	struct FFindOrCreateHandleResult
	{
		FGuid Handle;
		bool bWasCreated;
	};
	
	/**
	 * Finds or creates a binding to an object
	 *
	 * @param Object	The object to create a binding for
	 * @return A handle to the binding or an invalid handle if the object could not be bound
	 */
	FFindOrCreateHandleResult FindOrCreateHandleToObject( UObject* Object, bool bCreateHandleIfMissing = true )
	{
		FFindOrCreateHandleResult Result;
		bool bHandleWasValid = GetSequencer()->GetHandleToObject( Object, false ).IsValid();
		Result.Handle = GetSequencer()->GetHandleToObject( Object, bCreateHandleIfMissing );
		Result.bWasCreated = bHandleWasValid == false && Result.Handle.IsValid();
		return Result;
	}

	struct FFindOrCreateTrackResult
	{
		UMovieSceneTrack* Track;
		bool bWasCreated;
	};

	FFindOrCreateTrackResult FindOrCreateTrackForObject( const FGuid& ObjectHandle, TSubclassOf<UMovieSceneTrack> TrackClass, FName PropertyName = NAME_None, bool bCreateTrackIfMissing = true )
	{
		FFindOrCreateTrackResult Result;
		bool bTrackExisted;

		UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
		Result.Track = MovieScene->FindTrack( TrackClass, ObjectHandle, PropertyName );
		bTrackExisted = Result.Track != nullptr;

		if (!Result.Track && bCreateTrackIfMissing)
		{
			Result.Track = AddTrack(MovieScene, ObjectHandle, TrackClass, PropertyName);
		}

		Result.bWasCreated = bTrackExisted == false && Result.Track != nullptr;

		return Result;
	}

	template<typename TrackClass>
	struct FFindOrCreateMasterTrackResult
	{
		TrackClass* Track;
		bool bWasCreated;
	};

	/**
	 * Find or add a master track of the specified type in the focused movie scene.
	 *
	 * @param TrackClass The class of the track to find or add.
	 * @return The track results.
	 */
	template<typename TrackClass>
	FFindOrCreateMasterTrackResult<TrackClass> FindOrCreateMasterTrack()
	{
		FFindOrCreateMasterTrackResult<TrackClass> Result;
		bool bTrackExisted;

		UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
		Result.Track = MovieScene->FindMasterTrack<TrackClass>();
		bTrackExisted = Result.Track != nullptr;

		if (Result.Track == nullptr)
		{
			Result.Track = MovieScene->AddMasterTrack<TrackClass>();
		}

		Result.bWasCreated = bTrackExisted == false && Result.Track != nullptr;
		return Result;
	}


	/** @return The sequencer bound to this handler */
	const TSharedPtr<ISequencer> GetSequencer() const
	{
		return Sequencer.Pin();
	}

public:

	// ISequencerTrackEditor interface

	virtual void AddKey( const FGuid& ObjectGuid ) override {}

	virtual UMovieSceneTrack* AddTrack(UMovieScene* FocusedMovieScene, const FGuid& ObjectHandle, TSubclassOf<class UMovieSceneTrack> TrackClass, FName UniqueTypeName) override
	{
		return FocusedMovieScene->AddTrack(TrackClass, ObjectHandle);
	}

	virtual void BindCommands(TSharedRef<FUICommandList> SequencerCommandBindings) override { }
	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override { }
	virtual void BuildObjectBindingEditButtons(TSharedPtr<SHorizontalBox> EditBox, const FGuid& ObjectBinding, const UClass* ObjectClass) override { }
	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass) override { }
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override  { return TSharedPtr<SWidget>(); }
	virtual void BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track ) override { }
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override { return false; }

	virtual bool IsAllowedKeyAll() const
	{
		return Sequencer.Pin()->GetKeyAllEnabled();
	}

	virtual bool IsAllowedToAutoKey() const
	{
		// @todo sequencer livecapture: This turns on "auto key" for the purpose of capture keys for actor state
		// during PIE sessions when record mode is active.
		return Sequencer.Pin()->IsRecordingLive() || Sequencer.Pin()->GetAutoKeyMode() != EAutoKeyMode::KeyNone;
	}

	virtual void OnInitialize() override { };
	virtual void OnRelease() override { };

	virtual int32 PaintTrackArea(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle) override
	{
		return LayerId;
	}

	virtual bool SupportsType( TSubclassOf<class UMovieSceneTrack> TrackClass ) const = 0;
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const { return true; }
	virtual void Tick(float DeltaTime) override { }
	virtual EMultipleRowMode GetMultipleRowMode() const { return EMultipleRowMode::SingleTrack; }

protected:

	/**
	 * Gets the currently focused movie scene, if any.
	 *
	 * @return Focused movie scene, or nullptr if no movie scene is focused.
	 */
	UMovieScene* GetFocusedMovieScene() const
	{
		UMovieSceneSequence* FocusedSequence = GetSequencer()->GetFocusedMovieSceneSequence();
		return FocusedSequence->GetMovieScene();
	}

private:

	/** The sequencer bound to this handler.  Used to access movie scene and time info during auto-key */
	TWeakPtr<ISequencer> Sequencer;
};
