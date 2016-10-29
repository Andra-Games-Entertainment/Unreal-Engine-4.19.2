// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneFwd.h"

class UMovieSceneSequence;
class IMovieScenePlayer;

/**
 * A sequence instance holds the live objects bound to the tracks in a sequence.  This is completely transient
 * There is one instance per sequence.  If a sequence holds multiple sub-sequences each sub-sequence will have it's own instance even if they are the same sequence asset
 * A sequence instance also creates and manages all track instances for the tracks in a sequence
 */
class MOVIESCENE_API FMovieSceneSequenceInstance
	: public TSharedFromThis<FMovieSceneSequenceInstance>
{
public:
	
	FMovieSceneSequenceInstance(const UMovieSceneSequence& InMovieSceneSequence, FMovieSceneSequenceIDRef InSequenceID);

	DEPRECATED(4.15, "Please use IMovieScenePlayer::State.FindObjectId")
	FGuid FindObjectId(UObject& Object) const { return FGuid();}

	DEPRECATED(4.15, "Please use IMovieScenePlayer::State.FindObjectId")
	FGuid FindParentObjectId(UObject& Object) const { return FGuid(); }
	
	DEPRECATED(4.15, "Please use IMovieScenePlayer::State.FindBoundObjects")
	UObject* FindObject(const FGuid& ObjectId, const IMovieScenePlayer& Player) const {return nullptr;}

	DEPRECATED(4.15, "Please use IMovieScenePlayer::SpawnRegister.FindSpawnedObject")
	UObject* FindSpawnedObject(const FGuid& ObjectId) const{return nullptr;}

	DEPRECATED(4.15, "Direct access to the sequence at runtime should no longer be necessary.")
	UMovieSceneSequence* GetSequence() const { return MovieSceneSequence.Get(); }

	DEPRECATED(4.15, "Direct access to the sequence's time range should no longer be necessary.")
	TRange<float> GetTimeRange() const;

	FMovieSceneSequenceIDRef GetSequenceID() const { return SequenceID; }

private:
	TWeakObjectPtr<UMovieSceneSequence> MovieSceneSequence;
	FMovieSceneSequenceID SequenceID;
};
