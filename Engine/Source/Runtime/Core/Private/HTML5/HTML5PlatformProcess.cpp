// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HTML5Process.cpp: HTML5 implementations of Process functions
=============================================================================*/

#include "HTML5/HTML5PlatformProcess.h"
#include "Misc/CoreStats.h"
#include "Misc/App.h"
#include "Misc/SingleThreadEvent.h"

const TCHAR* FHTML5PlatformProcess::ComputerName()
{
	return TEXT("Browser");
}

const TCHAR* FHTML5PlatformProcess::BaseDir()
{
	return TEXT("");
}

DECLARE_CYCLE_STAT(TEXT("CPU Stall - HTML5Sleep"),STAT_HTML5Sleep,STATGROUP_CPUStalls);

void FHTML5PlatformProcess::Sleep( float Seconds )
{
	SCOPE_CYCLE_COUNTER(STAT_HTML5Sleep);
	FThreadIdleStats::FScopeIdle Scope;

	SleepNoStats(Seconds);
	// @todo html5 threading: actually put the thread to sleep for awhile (UE3 did nothing, we do the same now)
}

void FHTML5PlatformProcess::SleepNoStats(float Seconds)
{
	// @todo html5 threading: actually put the thread to sleep for awhile (UE3 did nothing, we do the same now)
}

void FHTML5PlatformProcess::SleepInfinite()
{
	// stop this thread forever
	//pause();
}

#include "HTML5PlatformRunnableThread.h"

FRunnableThread* FHTML5PlatformProcess::CreateRunnableThread()
{
	return new FHTML5RunnableThread();
}

class FEvent* FHTML5PlatformProcess::CreateSynchEvent( bool bIsManualReset /*= 0*/ )
{
	FEvent* Event = new FSingleThreadEvent();
	return Event;
}

bool FHTML5PlatformProcess::SupportsMultithreading()
{
	return false;
}

void FHTML5PlatformProcess::LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error)
{
	auto TmpURL = StringCast<ANSICHAR>(URL);
	EM_ASM_ARGS({var InUrl = Pointer_stringify($0); console.log("Opening "+InUrl); window.open(InUrl);}, (ANSICHAR*)TmpURL.Get());
}

const TCHAR* FHTML5PlatformProcess::ExecutableName(bool bRemoveExtension)
{
	return FApp::GetProjectName();
}
