// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#if PLATFORM_WINDOWS
	#include "Windows/WindowsPlatformCodeAnalysis.h"
#elif PLATFORM_COMPILER_CLANG
	#include "Clang/ClangPlatformCodeAnalysis.h"
#endif

#ifndef USING_ADDRESS_SANITISER
#define USING_ADDRESS_SANITISER 0
#endif
