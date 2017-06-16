// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MacTargetSettings.h: Declares the UMacTargetSettings class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MacTargetSettings.generated.h"

UENUM()
enum class EMacMetalShaderStandard : uint8
{
    /** Metal Shaders Compatible With OS X El Capitan 10.11.4 or later (std=osx-metal1.1) */
    MacMetalSLStandard_1_1 = 1 UMETA(DisplayName="Metal v1.1 (10.11.4+)"),
    
    /** Metal Shaders, supporting Tessellation Shaders & Fragment Shader UAVs, Compatible With macOS Sierra 10.12.0 or later (std=osx-metal1.2) */
    MacMetalSLStandard_1_2 = 2 UMETA(DisplayName="Metal v1.2 (10.12.0+)"),
    
    /** Metal Shaders, supporting multiple viewports, Compatible With macOS 10.13.0 or later (std=osx-metal2.0) */
    MacMetalSLStandard_2_0 = 3 UMETA(DisplayName="Metal v2.0 (10.13.0+)"),
};

/**
 * Implements the settings for the Mac target platform.
 */
UCLASS(config=Engine, defaultconfig)
class MACTARGETPLATFORM_API UMacTargetSettings
	: public UObject
{
public:

	GENERATED_UCLASS_BODY()
	
	/**
	 * The collection of RHI's we want to support on this platform.
	 * This is not always the full list of RHI we can support.
	 */
	UPROPERTY(EditAnywhere, config, Category=Rendering)
	TArray<FString> TargetedRHIs;
    
    /**
     * The maximum supported Metal shader langauge version. 
     * This defines what features may be used and OS versions supported.
     */
    UPROPERTY(EditAnywhere, config, Category=Rendering, meta = (DisplayName = "Max. Metal Shader Standard To Target", ConfigRestartRequired = true))
    uint8 MaxShaderLanguageVersion;
    
    /**
     * Whether to use the Metal shading language's "fast" intrinsics.
	 * Fast intrinsics assume that no NaN or INF value will be provided as input, 
	 * so are more efficient. However, they will produce undefined results if NaN/INF 
	 * is present in the argument/s. By default fast-instrinics are disabled so Metal correctly handles NaN/INF arguments.
     */
    UPROPERTY(EditAnywhere, config, Category=Rendering, meta = (DisplayName = "Use Fast-Math intrinsics", ConfigRestartRequired = true))
	bool UseFastIntrinsics;
	
	/**
	 * Whether to use of Metal shader-compiler's -ffast-math optimisations.
	 * Fast-Math performs algebraic-equivalent & reassociative optimisations not permitted by the floating point arithmetic standard (IEEE-754).
	 * These can improve shader performance at some cost to precision and can lead to NaN/INF propagation as they rely on
	 * shader inputs or variables not containing NaN/INF values. By default fast-math is enabled for performance.
	 */
	UPROPERTY(EditAnywhere, config, Category=Rendering, meta = (DisplayName = "Enable Fast-Math optimisations", ConfigRestartRequired = true))
	bool EnableMathOptimisations;
};
