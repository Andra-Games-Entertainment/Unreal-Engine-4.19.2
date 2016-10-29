// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneActorReferenceSection.h"
#include "MovieSceneActorReferenceTrack.h"
#include "IMovieScenePlayer.h"
#include "Evaluation/MovieSceneActorReferenceTemplate.h"


UMovieSceneActorReferenceTrack::UMovieSceneActorReferenceTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }


UMovieSceneSection* UMovieSceneActorReferenceTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneActorReferenceSection::StaticClass(), NAME_None, RF_Transactional);
}

FMovieSceneEvalTemplatePtr UMovieSceneActorReferenceTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneActorReferenceSectionTemplate(*CastChecked<UMovieSceneActorReferenceSection>(&InSection), *this);
}

