// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneSlomoTemplate.h"
#include "MovieSceneSlomoTrack.h"
#include "MovieSceneSlomoSection.h"
#include "MovieSceneSequence.h"
#include "MovieSceneEvaluationTemplateInstance.h"

DECLARE_CYCLE_STAT(TEXT("Slomo Track Token Execute"), MovieSceneEval_SlomoTrack_TokenExecute, STATGROUP_MovieSceneEval);

struct FSlomoTrackToken
{
	FSlomoTrackToken(float InSlomoValue)
		: SlomoValue(InSlomoValue)
	{}

	float SlomoValue;

	void Apply(IMovieScenePlayer& Player)
	{
		UObject* PlaybackContext = Player.GetPlaybackContext();
		UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;

		if (!GIsEditor || 
			(World && World->GetWorld()->GetNetMode() == NM_Client) ||
			!GEngine ||
			SlomoValue <= 0.f)
		{
			return;
		}

		AWorldSettings* WorldSettings = World ? World->GetWorldSettings() : nullptr;
			
		if (WorldSettings)
		{	
			WorldSettings->MatineeTimeDilation = SlomoValue;
			WorldSettings->ForceNetUpdate();
		}
	}
};

struct FSlomoTrackData : IPersistentEvaluationData
{
	TOptional<FSlomoTrackToken> PreviousSlomoValue;
};

struct FSlomoPreAnimatedGlobalToken : FSlomoTrackToken, IMovieScenePreAnimatedGlobalToken
{
	FSlomoPreAnimatedGlobalToken(float InSlomoValue)
		: FSlomoTrackToken(InSlomoValue)
	{}

	virtual void RestoreState(IMovieScenePlayer& Player) override
	{
		Apply(Player);
	}
};

struct FSlomoPreAnimatedGlobalTokenProducer : IMovieScenePreAnimatedGlobalTokenProducer
{
	FSlomoPreAnimatedGlobalTokenProducer(IMovieScenePlayer& InPlayer) 
		: Player(InPlayer)
	{}

	virtual IMovieScenePreAnimatedGlobalTokenPtr CacheExistingState() const override
	{
		if (AWorldSettings* WorldSettings = Player.GetPlaybackContext()->GetWorld()->GetWorldSettings())
		{
			return FSlomoPreAnimatedGlobalToken(WorldSettings->MatineeTimeDilation);
		}
		return IMovieScenePreAnimatedGlobalTokenPtr();
	}

	IMovieScenePlayer& Player;
};

/** A movie scene execution token that applies slomo */
struct FSlomoExecutionToken : IMovieSceneExecutionToken, FSlomoTrackToken
{
	FSlomoExecutionToken(float InSlomoValue)
		: FSlomoTrackToken(InSlomoValue)
	{}

	static FMovieSceneAnimTypeID GetAnimTypeID()
	{
		return TMovieSceneAnimTypeID<FSlomoExecutionToken>();
	}
	
	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_SlomoTrack_TokenExecute)

		Player.SavePreAnimatedState(GetAnimTypeID(), FSlomoPreAnimatedGlobalTokenProducer(Player));
		
		Apply(Player);
	}
};

FMovieSceneSlomoSectionTemplate::FMovieSceneSlomoSectionTemplate(const UMovieSceneSlomoSection& Section)
	: SlomoCurve(Section.GetFloatCurve())
{
}

void FMovieSceneSlomoSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	float SlomoValue = SlomoCurve.Eval(Context.GetTime());
	if (SlomoValue >= 0.f)
	{
		ExecutionTokens.Add(FSlomoExecutionToken(SlomoValue));
	}
}