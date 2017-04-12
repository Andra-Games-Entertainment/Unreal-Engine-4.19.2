// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#define USE_VS_HS_ATTRIBUTES 1

#include "hlslcc.h"
THIRD_PARTY_INCLUDES_START
	#include "ir.h"
THIRD_PARTY_INCLUDES_END
#include "PackUniformBuffers.h"
#include "LanguageSpec.h"


class FMetalLanguageSpec : public ILanguageSpec
{
public:
	virtual bool SupportsDeterminantIntrinsic() const override
	{
		//@todo-rco: Temp workaround for Seed 2 & 3
		return false;// true;
	}

	virtual bool SupportsTransposeIntrinsic() const override
	{
		//@todo-rco: Temp workaround for Seed 2 & 3
		return false;// true;
	}
	virtual bool SupportsIntegerModulo() const override { return true; }

	virtual bool SupportsMatrixConversions() const override { return false; }

	virtual void SetupLanguageIntrinsics(_mesa_glsl_parse_state* State, exec_list* ir) override;

	virtual bool AllowsSharingSamplers() const override { return true; }

	virtual bool UseSamplerInnerType() const override { return true; }

	virtual bool CanConvertBetweenHalfAndFloat() const override { return false; }

	virtual bool NeedsAtomicLoadStore() const override { return true; }
	
	virtual bool SplitInputVariableStructs() const { return false; }
};

struct FBuffers;
struct FMetalTessellationOutputs;

enum EMetalAccess
{
    EMetalAccessRead = 1,
    EMetalAccessWrite = 2,
    EMetalAccessReadWrite = 3,
};

enum EMetalGPUSemantics
{
	EMetalGPUSemanticsMobile, // Mobile shaders for TBDR GPUs
	EMetalGPUSemanticsTBDRDesktop, // Desktop shaders for TBDR GPUs
	EMetalGPUSemanticsImmediateDesktop // Desktop shaders for Immediate GPUs
};

// Generates Metal compliant code from IR tokens
struct FMetalCodeBackend : public FCodeBackend
{
	FMetalCodeBackend(FMetalTessellationOutputs& Attribs, unsigned int InHlslCompileFlags, EHlslCompileTarget InTarget, uint8 Version, EMetalGPUSemantics bInDesktop, bool bInZeroInitialise, bool bInBoundsChecks);

	virtual char* GenerateCode(struct exec_list* ir, struct _mesa_glsl_parse_state* ParseState, EHlslShaderFrequency Frequency) override;

	virtual bool GenerateMain(EHlslShaderFrequency Frequency, const char* EntryPoint, exec_list* Instructions, _mesa_glsl_parse_state* ParseState) override;

	void CallPatchConstantFunction(_mesa_glsl_parse_state* ParseState, ir_variable* OutputPatchVar, ir_variable* internalPatchIDVar, ir_function_signature* PatchConstantSig, exec_list& DeclInstructions, exec_list &PostCallInstructions, int &onAttribute);

	// Return false if there were restrictions that made compilation fail
	virtual bool ApplyAndVerifyPlatformRestrictions(exec_list* Instructions, _mesa_glsl_parse_state* ParseState, EHlslShaderFrequency Frequency) override;

	void PackInputsAndOutputs(exec_list* ir, _mesa_glsl_parse_state* state, EHlslShaderFrequency Frequency, exec_list& InputVars);
	void MovePackedUniformsToMain(exec_list* ir, _mesa_glsl_parse_state* state, FBuffers& OutBuffers);
	void FixIntrinsics(exec_list* ir, _mesa_glsl_parse_state* state);
	void RemovePackedVarReferences(exec_list* ir, _mesa_glsl_parse_state* State);
	void PromoteInputsAndOutputsGlobalHalfToFloat(exec_list* ir, _mesa_glsl_parse_state* state, EHlslShaderFrequency Frequency);
	void ConvertHalfToFloatUniformsAndSamples(exec_list* ir, _mesa_glsl_parse_state* State, bool bConvertUniforms, bool bConvertSamples);
	void BreakPrecisionChangesVisitor(exec_list* ir, _mesa_glsl_parse_state* State);

    TMap<ir_variable*, uint32> ImageRW;
    FMetalTessellationOutputs& TessAttribs;
    
    uint8 Version;
	EMetalGPUSemantics bIsDesktop;
	bool bZeroInitialise;
	bool bBoundsChecks;

	bool bIsTessellationVSHS = false;
	unsigned int inputcontrolpoints = 0;
	unsigned int patchesPerThreadgroup = 0;
};
