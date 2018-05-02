// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
// ...

#include "VulkanShaderFormat.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/IShaderFormatModule.h"
#include "hlslcc.h"
#include "ShaderCore.h"

static FName NAME_VULKAN_ES3_1_ANDROID(TEXT("SF_VULKAN_ES31_ANDROID"));
static FName NAME_VULKAN_ES3_1_ANDROID_UB(TEXT("SF_VULKAN_ES31_ANDROID_UB"));
static FName NAME_VULKAN_ES3_1(TEXT("SF_VULKAN_ES31"));
static FName NAME_VULKAN_ES3_1_UB(TEXT("SF_VULKAN_ES31_UB"));
static FName NAME_VULKAN_SM4_NOUB(TEXT("SF_VULKAN_SM4_NOUB"));
static FName NAME_VULKAN_SM4(TEXT("SF_VULKAN_SM4"));
static FName NAME_VULKAN_SM5_NOUB(TEXT("SF_VULKAN_SM5_NOUB"));
static FName NAME_VULKAN_SM5(TEXT("SF_VULKAN_SM5"));

class FShaderFormatVulkan : public IShaderFormat
{
	enum 
	{
		UE_SHADER_VULKAN_ES3_1_VER = 14,
		UE_SHADER_VULKAN_SM4_VER = 14,
		UE_SHADER_VULKAN_SM5_VER = 14,
	};

	int32 InternalGetVersion(FName Format) const
	{
		if (Format == NAME_VULKAN_SM4 || Format == NAME_VULKAN_SM4_NOUB)
		{
			return UE_SHADER_VULKAN_SM4_VER;
		}
		else if (Format == NAME_VULKAN_SM5 || Format == NAME_VULKAN_SM5_NOUB)
		{
			return UE_SHADER_VULKAN_SM5_VER;
		}
		else if (Format == NAME_VULKAN_ES3_1_ANDROID || Format == NAME_VULKAN_ES3_1_ANDROID_UB || Format == NAME_VULKAN_ES3_1 || Format == NAME_VULKAN_ES3_1_UB)
		{
			return UE_SHADER_VULKAN_ES3_1_VER;
		}
		else if (Format == NAME_VULKAN_ES3_1_UB)
		{
			return UE_SHADER_VULKAN_ES3_1_VER;
		}

		check(0);
		return -1;
	}

public:
	virtual uint32 GetVersion(FName Format) const override
	{
		const uint8 HLSLCCVersion = ((HLSLCC_VersionMajor & 0x0f) << 4) | (HLSLCC_VersionMinor & 0x0f);
		const uint16 Version = ((HLSLCCVersion & 0xff) << 8) | (InternalGetVersion(Format) & 0xff);
		return Version;
	}
	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const
	{
		OutFormats.Add(NAME_VULKAN_SM4);
		OutFormats.Add(NAME_VULKAN_SM5);
		OutFormats.Add(NAME_VULKAN_ES3_1_ANDROID);
		OutFormats.Add(NAME_VULKAN_ES3_1_ANDROID_UB);
		OutFormats.Add(NAME_VULKAN_ES3_1);
		OutFormats.Add(NAME_VULKAN_ES3_1_UB);
		OutFormats.Add(NAME_VULKAN_SM4_NOUB);
		OutFormats.Add(NAME_VULKAN_SM5_NOUB);
	}

	virtual void CompileShader(FName Format, const struct FShaderCompilerInput& Input, struct FShaderCompilerOutput& Output,const FString& WorkingDirectory) const
	{
		check(InternalGetVersion(Format) >= 0);
		if (Format == NAME_VULKAN_ES3_1)
		{
			CompileShader_Windows_Vulkan(Input, Output, WorkingDirectory, EVulkanShaderVersion::ES3_1);
		}
		else if (Format == NAME_VULKAN_ES3_1_UB)
		{
			CompileShader_Windows_Vulkan(Input, Output, WorkingDirectory, EVulkanShaderVersion::ES3_1_UB);
		}
		else if (Format == NAME_VULKAN_ES3_1_ANDROID)
		{
			CompileShader_Windows_Vulkan(Input, Output, WorkingDirectory, EVulkanShaderVersion::ES3_1_ANDROID);
		}
		else if (Format == NAME_VULKAN_ES3_1_ANDROID_UB)
		{
			CompileShader_Windows_Vulkan(Input, Output, WorkingDirectory, EVulkanShaderVersion::ES3_1_ANDROID_UB);
		}
		else if (Format == NAME_VULKAN_SM4_NOUB)
		{
			CompileShader_Windows_Vulkan(Input, Output, WorkingDirectory, EVulkanShaderVersion::SM4_NOUB);
		}
		else if (Format == NAME_VULKAN_SM4)
		{
			CompileShader_Windows_Vulkan(Input, Output, WorkingDirectory, EVulkanShaderVersion::SM4);
		}
		else if (Format == NAME_VULKAN_SM5_NOUB)
		{
			CompileShader_Windows_Vulkan(Input, Output, WorkingDirectory, EVulkanShaderVersion::SM5_NOUB);
		}
		else if (Format == NAME_VULKAN_SM5)
		{
			CompileShader_Windows_Vulkan(Input, Output, WorkingDirectory, EVulkanShaderVersion::SM5);
		}
	}

	//virtual bool CreateLanguage(FName Format, ILanguageSpec*& OutSpec, FCodeBackend*& OutBackend, uint32 InHlslCompileFlags) override
	//{
	//	OutSpec = new FVulkanLanguageSpec(false);
	//	OutBackend = new FVulkanCodeBackend(InHlslCompileFlags, HCT_FeatureLevelSM4);
	//	return false;
	//}
};

/**
 * Module for Vulkan shaders
 */

static IShaderFormat* Singleton = NULL;

class FVulkanShaderFormatModule : public IShaderFormatModule
{
public:
	virtual ~FVulkanShaderFormatModule()
	{
		delete Singleton;
		Singleton = NULL;
	}
	virtual IShaderFormat* GetShaderFormat()
	{
		if (!Singleton)
		{
			Singleton = new FShaderFormatVulkan();
		}
		return Singleton;
	}
};

IMPLEMENT_MODULE( FVulkanShaderFormatModule, VulkanShaderFormat);
