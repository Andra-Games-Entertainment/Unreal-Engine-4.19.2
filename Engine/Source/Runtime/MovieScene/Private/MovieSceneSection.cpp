// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "MovieSceneCommonHelpers.h"
#include "Evaluation/MovieSceneEvalTemplate.h"


UMovieSceneSection::UMovieSceneSection(const FObjectInitializer& ObjectInitializer)
	: Super( ObjectInitializer )
	, StartTime(0.0f)
	, EndTime(0.0f)
	, RowIndex(0)
	, OverlapPriority(0)
	, bIsActive(true)
	, bIsLocked(false)
	, bIsInfinite(false)
	, PreRollTime(0)
	, PostRollTime(0)
{ }


void UMovieSceneSection::PostInitProperties()
{
	// Propagate sub object flags from our outer (track) to ourselves. This is required for sections that are stored on blueprints (archetypes) so that they can be referenced in worlds.
	if (GetOuter()->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject))
	{
		SetFlags(GetOuter()->GetMaskedFlags(RF_PropagateToSubObjects));
	}
	
	Super::PostInitProperties();
}


bool UMovieSceneSection::TryModify(bool bAlwaysMarkDirty)
{
	if (IsLocked())
	{
		return false;
	}

	Modify(bAlwaysMarkDirty);

	return true;
}


const UMovieSceneSection* UMovieSceneSection::OverlapsWithSections(const TArray<UMovieSceneSection*>& Sections, int32 TrackDelta, float TimeDelta) const
{
	// Check overlaps with exclusive ranges so that sections can butt up against each other
	int32 NewTrackIndex = RowIndex + TrackDelta;
	TRange<float> NewSectionRange = TRange<float>(TRange<float>::BoundsType::Exclusive(StartTime + TimeDelta), TRange<float>::BoundsType::Exclusive(EndTime + TimeDelta));

	for (const auto Section : Sections)
	{
		check(Section);
		if ((this != Section) && (Section->GetRowIndex() == NewTrackIndex))
		{
			TRange<float> ExclusiveSectionRange = TRange<float>(TRange<float>::BoundsType::Exclusive(Section->GetRange().GetLowerBoundValue()), TRange<float>::BoundsType::Exclusive(Section->GetRange().GetUpperBoundValue()));
			if (NewSectionRange.Overlaps(ExclusiveSectionRange))
			{
				return Section;
			}
		}
	}

	return nullptr;
}


void UMovieSceneSection::InitialPlacement(const TArray<UMovieSceneSection*>& Sections, float InStartTime, float InEndTime, bool bAllowMultipleRows)
{
	check(StartTime <= EndTime);

	StartTime = InStartTime;
	EndTime = InEndTime;
	RowIndex = 0;

	if (bAllowMultipleRows)
	{
		while (OverlapsWithSections(Sections) != nullptr)
		{
			++RowIndex;
		}
	}
	else
	{
		for (;;)
		{
			const UMovieSceneSection* OverlappedSection = OverlapsWithSections(Sections);

			if (OverlappedSection == nullptr)
			{
				break;
			}

			TSet<FKeyHandle> KeyHandles;
			MoveSection(OverlappedSection->GetEndTime() - StartTime, KeyHandles);
		}
	}
}


UMovieSceneSection* UMovieSceneSection::SplitSection(float SplitTime)
{
	if (!IsTimeWithinSection(SplitTime))
	{
		return nullptr;
	}

	SetFlags(RF_Transactional);

	if (TryModify())
	{
		float SectionEndTime = GetEndTime();
				
		// Trim off the right
		SetEndTime(SplitTime);

		// Create a new section
		UMovieSceneTrack* Track = CastChecked<UMovieSceneTrack>(GetOuter());
		Track->Modify();

		UMovieSceneSection* NewSection = DuplicateObject<UMovieSceneSection>(this, Track);
		check(NewSection);

		NewSection->SetStartTime(SplitTime);
		NewSection->SetEndTime(SectionEndTime);
		Track->AddSection(*NewSection);

		return NewSection;
	}

	return nullptr;
}


void UMovieSceneSection::TrimSection(float TrimTime, bool bTrimLeft)
{
	if (IsTimeWithinSection(TrimTime))
	{
		SetFlags(RF_Transactional);
		if (TryModify())
		{
			if (bTrimLeft)
			{
				SetStartTime(TrimTime);
			}
			else
			{
				SetEndTime(TrimTime);
			}
		}
	}
}


void UMovieSceneSection::AddKeyToCurve(FRichCurve& InCurve, float Time, float Value, EMovieSceneKeyInterpolation Interpolation, const bool bUnwindRotation)
{
	if (IsTimeWithinSection(Time))
	{
		if (TryModify())
		{
			FKeyHandle ExistingKeyHandle = InCurve.FindKey(Time);
			FKeyHandle NewKeyHandle = InCurve.UpdateOrAddKey(Time, Value, bUnwindRotation);

			if (!InCurve.IsKeyHandleValid(ExistingKeyHandle) && InCurve.IsKeyHandleValid(NewKeyHandle))
			{
				MovieSceneHelpers::SetKeyInterpolation(InCurve, NewKeyHandle, Interpolation);
			}
		}
	}
}


void UMovieSceneSection::SetCurveDefault(FRichCurve& InCurve, float Value)
{
	if (InCurve.GetDefaultValue() != Value && TryModify())
	{
		InCurve.SetDefaultValue(Value);
	}
}


FMovieSceneEvalTemplatePtr UMovieSceneSection::GenerateTemplate() const
{
	return FMovieSceneEvalTemplatePtr();
}
