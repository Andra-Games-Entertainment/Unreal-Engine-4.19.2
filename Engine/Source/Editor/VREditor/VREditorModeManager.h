// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/GCObject.h"
#include "Kismet/HeadMountedDisplayFunctionLibrary.h"

class UVREditorMode;
enum class EMapChangeType : uint8;

/**
 * Manages starting and closing the VREditor mode
 */
class FVREditorModeManager : public FGCObject
{

public:

	/** Default constructor */
	FVREditorModeManager();

	/** Default destructor */
	~FVREditorModeManager();

	/** Ticks to detect entering and closing the VR Editor */
	void Tick( const float DeltaTime );

	/** Start or stop the VR Editor */
	void EnableVREditor( const bool bEnable, const bool bForceWithoutHMD );

	/** If the VR Editor is currently running */
	bool IsVREditorActive() const;

	/** If the VR Editor is currently available */
	bool IsVREditorAvailable() const;

	/** Gets the current VR Editor mode that was enabled */
	UVREditorMode* GetCurrentVREditorMode();

	// FGCObject
	virtual void AddReferencedObjects( FReferenceCollector& Collector );
	// End FGCObject

private:

	/** Saves the WorldToMeters and enters the mode belonging to GWorld */
	void StartVREditorMode( const bool bForceWithoutHMD );

	/** Closes the current VR Editor if any and sets the WorldToMeters to back to the one from before entering the VR mode */
	void CloseVREditor(const bool bShouldDisableStereo );

	/** Directly set the GWorld WorldToMeters */
	void SetDirectWorldToMeters( const float NewWorldToMeters );
	
	/** On level changed */
	void OnMapChanged( UWorld* World, EMapChangeType MapChangeType );
	
	/** The current mode, nullptr if none */
	UPROPERTY()
	UVREditorMode* CurrentVREditorMode;

	/** The previous mode, nullptr if none */
	UPROPERTY()
	UVREditorMode* PreviousVREditorMode;

	/** If the VR Editor mode needs to be enabled next tick */
	bool bEnableVRRequest;

	/** True when we detect that the user is wearing the HMD */
	EHMDWornState::Type HMDWornState;

	/** Timer for checking if the user is wearing the HMD */
	float TimeSinceHMDChecked;
};
