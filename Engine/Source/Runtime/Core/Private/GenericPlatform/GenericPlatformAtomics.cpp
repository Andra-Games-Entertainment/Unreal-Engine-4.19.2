// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/GenericPlatformAtomics.h"
#include "Templates/AlignOf.h"


static_assert(sizeof(FInt128) == 16, "FInt128 size must be 16 bytes.");
static_assert(ALIGNOF(FInt128) == 16, "FInt128 alignment must equals 16.");
