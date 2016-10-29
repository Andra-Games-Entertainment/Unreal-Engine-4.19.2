// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneIntegerSection.h"
#include "MovieSceneIntegerTrack.h"
#include "IMovieScenePlayer.h"
#include "Evaluation/MovieScenePropertyTemplates.h"

UMovieSceneIntegerTrack::UMovieSceneIntegerTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }


UMovieSceneSection* UMovieSceneIntegerTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneIntegerSection::StaticClass(), NAME_None, RF_Transactional);
}

FMovieSceneEvalTemplatePtr UMovieSceneIntegerTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneIntegerPropertySectionTemplate(*CastChecked<UMovieSceneIntegerSection>(&InSection), *this);
}
