// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Transform.h"
#include "CriticalSection.h"
#include "TickableEditorObject.h"

#include "ILiveLinkClient.h"
#include "ILiveLinkSource.h"
#include "LiveLinkRefSkeleton.h"
#include "LiveLinkTypes.h"
#include "LiveLinkConnectionSettings.h"

/** Delegate called when the state the client sources has changed. */
DECLARE_MULTICAST_DELEGATE(FLiveLinkSourcesChanged);

struct FLiveLinkFrame
{
public:
	TArray<FTransform>		Transforms;
	TArray<FOptionalCurveElement>	Curves;

	FLiveLinkTimeCode TimeCode;

	void ExtendCurveData(int32 ExtraCurves)
	{
		Curves.AddDefaulted(ExtraCurves);
	}
};

struct FLiveLinkSubject
{
	// Ref Skeleton for transforms
	FLiveLinkRefSkeleton RefSkeleton;

	// Key for storing curve data (Names)
	FLiveLinkCurveKey	 CurveKeyData;

	// Subject data frames that we have received (transforms and curve values)
	TArray<FLiveLinkFrame>		Frames;

	// Time difference between current system time and TimeCode times 
	double SubjectTimeOffset;

	// Last time we read a frame from this subject. Used to determine whether any new incoming
	// frames are useable
	double LastReadTime;

	//Cache of the last frame we read from, Used for frame cleanup
	int32 LastReadFrame;

	// Guid to track the last live link source that modified us
	FGuid LastModifier;

	// Connection settings specified by user
	FLiveLinkConnectionSettings CachedConnectionSettings;

	FLiveLinkSubject(const FLiveLinkRefSkeleton& InRefSkeleton)
		: RefSkeleton(InRefSkeleton)
	{}

	FLiveLinkSubject()
	{}

	// Add a frame of data to the subject
	void AddFrame(const TArray<FTransform>& Transforms, const TArray<FLiveLinkCurveElement>& CurveElements, const FLiveLinkTimeCode& TimeCode, FGuid FrameSource);

	// Populate OutFrame with a frame based off of the supplied time and our own offsets
	void BuildInterpolatedFrame(const double InSeconds, FLiveLinkSubjectFrame& OutFrame);
};

class FLiveLinkClient : public ILiveLinkClient, public FTickableEditorObject
{
public:
	FLiveLinkClient() : LastValidationCheck(0.0) {}
	~FLiveLinkClient();

	// Begin FTickableObjectBase implementation
	virtual bool IsTickable() const override { return true; }
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(LiveLinkClient, STATGROUP_Tickables); }
	// End FTickableObjectBase

	void AddSource(ILiveLinkSource* InSource);

	// ILiveLinkClient Interface
	virtual FLiveLinkTimeCode MakeTimeCode(double InTime, int32 InFrameNum) const;
	virtual FLiveLinkTimeCode MakeTimeCodeFromTimeOnly(double InTime) const;

	virtual void PushSubjectSkeleton(FName SubjectName, const FLiveLinkRefSkeleton& RefSkeleton) override;
	virtual void ClearSubject(FName SubjectName) override;
	virtual void PushSubjectData(FGuid SourceGuid, FName SubjectName, const TArray<FTransform>& Transforms, const TArray<FLiveLinkCurveElement>& CurveElements, const FLiveLinkTimeCode& TimeCode) override;
	// End ILiveLinkClient Interface

	virtual const FLiveLinkSubjectFrame* GetSubjectData(FName SubjectName) override;

	const TArray<ILiveLinkSource*>& GetSources() const { return Sources; }
	const TArray<FGuid>& GetSourceEntries() const { return SourceGuids; }

	FText GetSourceTypeForEntry(FGuid InEntryGuid) const;
	FText GetMachineNameForEntry(FGuid InEntryGuid) const;
	FText GetEntryStatusForEntry(FGuid InEntryGuid) const;

	FLiveLinkConnectionSettings* GetConnectionSettingsForEntry(FGuid InEntryGuid);

	// Functions for managing sources changed delegate
	FDelegateHandle RegisterSourcesChangedHandle(const FLiveLinkSourcesChanged::FDelegate& SourcesChanged);
	void UnregisterSourcesChangedHandle(FDelegateHandle Handle);

private:

	// Get index of specified source
	int32 GetSourceIndexForGUID(FGuid InEntryGuid) const;

	// Get specified live link source
	ILiveLinkSource* GetSourceForGUID(FGuid InEntryGuid) const;

	// Test currently added sources to make sure they are still valid
	void ValidateSources();

	// Build subject data so that during the rest of the tick it can be read without
	// thread locking or mem copying
	void BuildThisTicksSubjectSnapshot();

	// Current streamed data for subjects
	TMap<FName, FLiveLinkSubject> LiveSubjectData;

	// Built snapshot of streamed subject data (updated once a tick)
	TMap<FName, FLiveLinkSubjectFrame> ActiveSubjectSnapshots;

	// Current Sources
	TArray<ILiveLinkSource*> Sources;
	TArray<FGuid>			 SourceGuids;
	TArray<FLiveLinkConnectionSettings> ConnectionSettings;

	// Cache last time we checked the validity of our sources 
	double LastValidationCheck;

	// Lock to stop multiple threads accessing the subject data map at the same time
	FCriticalSection SubjectDataAccessCriticalSection;

	// Delegate to notify interested parties when the client sources have changed
	FLiveLinkSourcesChanged OnLiveLinkSourcesChanged;
};