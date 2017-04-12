// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VolumetricFog.cpp
=============================================================================*/

#include "VolumetricFog.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "GlobalDistanceField.h"
#include "GlobalDistanceFieldParameters.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "DistanceFieldLightingShared.h"
#include "VolumetricFogShared.h"
#include "VolumeRendering.h"
#include "ScreenRendering.h"
#include "VolumeLighting.h"
#include "PipelineStateCache.h"

int32 GVolumetricFog = 1;
FAutoConsoleVariableRef CVarVolumetricFog(
	TEXT("r.VolumetricFog"),
	GVolumetricFog,
	TEXT("Whether to allow the volumetric fog feature."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GVolumetricFogInjectShadowedLightsSeparately = 1;
FAutoConsoleVariableRef CVarVolumetricFogInjectShadowedLightsSeparately(
	TEXT("r.VolumetricFog.InjectShadowedLightsSeparately"),
	GVolumetricFogInjectShadowedLightsSeparately,
	TEXT("Whether to allow the volumetric fog feature."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GVolumetricFogDepthDistributionScale = 32.0f;
FAutoConsoleVariableRef CVarVolumetricFogDepthDistributionScale(
	TEXT("r.VolumetricFog.DepthDistributionScale"),
	GVolumetricFogDepthDistributionScale,
	TEXT("Scales the slice depth distribution."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GVolumetricFogGridPixelSize = 16;
FAutoConsoleVariableRef CVarVolumetricFogGridPixelSize(
	TEXT("r.VolumetricFog.GridPixelSize"),
	GVolumetricFogGridPixelSize,
	TEXT("XY Size of a cell in the voxel grid, in pixels."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GVolumetricFogGridSizeZ = 64;
FAutoConsoleVariableRef CVarVolumetricFogGridSizeZ(
	TEXT("r.VolumetricFog.GridSizeZ"),
	GVolumetricFogGridSizeZ,
	TEXT("How many Volumetric Fog cells to use in z."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GVolumetricFogTemporalReprojection = 1;
FAutoConsoleVariableRef CVarVolumetricFogTemporalReprojection(
	TEXT("r.VolumetricFog.TemporalReprojection"),
	GVolumetricFogTemporalReprojection,
	TEXT("Whether to use temporal reprojection on volumetric fog."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GVolumetricFogJitter = 1;
FAutoConsoleVariableRef CVarVolumetricFogJitter(
	TEXT("r.VolumetricFog.Jitter"),
	GVolumetricFogJitter,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GVolumetricFogHistoryWeight = .9f;
FAutoConsoleVariableRef CVarVolumetricFogHistoryWeight(
	TEXT("r.VolumetricFog.HistoryWeight"),
	GVolumetricFogHistoryWeight,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GInverseSquaredLightDistanceBiasScale = 1.0f;
FAutoConsoleVariableRef CVarInverseSquaredLightDistanceBiasScale(
	TEXT("r.VolumetricFog.InverseSquaredLightDistanceBiasScale"),
	GInverseSquaredLightDistanceBiasScale,
	TEXT("Scales the amount added to the inverse squared falloff denominator.  This effectively removes the spike from inverse squared falloff that causes extreme aliasing."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FVolumetricFogGlobalData,TEXT("VolumetricFog"));

FVolumetricFogGlobalData::FVolumetricFogGlobalData()
{}

float TemporalHalton( int32 Index, int32 Base )
{
	float Result = 0.0f;
	float InvBase = 1.0f / Base;
	float Fraction = InvBase;
	while( Index > 0 )
	{
		Result += ( Index % Base ) * Fraction;
		Index /= Base;
		Fraction *= InvBase;
	}
	return Result;
}

FVector VolumetricFogTemporalRandom(uint32 FrameNumber)
{
	// Center of the voxel
	FVector RandomOffsetValue(.5f, .5f, .5f);

	if (GVolumetricFogJitter && GVolumetricFogTemporalReprojection)
	{
		RandomOffsetValue = FVector(TemporalHalton(FrameNumber & 1023, 2), TemporalHalton(FrameNumber & 1023, 3), TemporalHalton(FrameNumber & 1023, 5));
	}

	return RandomOffsetValue;
}

uint32 VolumetricFogGridInjectionGroupSize = 4;

class FVolumetricFogMaterialSetupCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVolumetricFogMaterialSetupCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return DoesPlatformSupportVolumetricFog(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), VolumetricFogGridInjectionGroupSize);
		FVolumetricFogIntegrationParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FVolumetricFogMaterialSetupCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		VolumetricFogParameters.Bind(Initializer.ParameterMap);
		HeightFogParameters.Bind(Initializer.ParameterMap);
		GlobalAlbedo.Bind(Initializer.ParameterMap, TEXT("GlobalAlbedo"));
		GlobalExtinctionScale.Bind(Initializer.ParameterMap, TEXT("GlobalExtinctionScale"));
	} 

	FVolumetricFogMaterialSetupCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View, 
		const FExponentialHeightFogSceneInfo& FogInfo,
		IPooledRenderTarget* VBufferA,
		IPooledRenderTarget* VBufferB)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		VolumetricFogParameters.Set(RHICmdList, ShaderRHI, View, VBufferA, VBufferB, NULL);
		HeightFogParameters.Set(RHICmdList, ShaderRHI, &View);
		SetShaderValue(RHICmdList, ShaderRHI, GlobalAlbedo, FogInfo.VolumetricFogAlbedo);
		SetShaderValue(RHICmdList, ShaderRHI, GlobalExtinctionScale, FogInfo.VolumetricFogExtinctionScale);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		IPooledRenderTarget* VBufferA,
		IPooledRenderTarget* VBufferB)
	{
		VolumetricFogParameters.UnsetParameters(RHICmdList, GetComputeShader(), View, VBufferA, VBufferB, NULL, false);
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << VolumetricFogParameters;
		Ar << HeightFogParameters;
		Ar << GlobalAlbedo;
		Ar << GlobalExtinctionScale;
		return bShaderHasOutdatedParameters;
	}

private:

	FVolumetricFogIntegrationParameters VolumetricFogParameters;
	FExponentialHeightFogShaderParameters HeightFogParameters;
	FShaderParameter GlobalAlbedo;
	FShaderParameter GlobalExtinctionScale;
};

IMPLEMENT_SHADER_TYPE(,FVolumetricFogMaterialSetupCS,TEXT("VolumetricFog"),TEXT("MaterialSetupCS"),SF_Compute);

/** Vertex shader used to write to a range of slices of a 3d volume texture. */
class FWriteToBoundingSphereVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FWriteToBoundingSphereVS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) 
	{ 
		return DoesPlatformSupportVolumetricFog(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_VertexToGeometryShader);
	}

	FWriteToBoundingSphereVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		MinZ.Bind(Initializer.ParameterMap, TEXT("MinZ"));
		ViewSpaceBoundingSphere.Bind(Initializer.ParameterMap, TEXT("ViewSpaceBoundingSphere"));
		ViewToVolumeClip.Bind(Initializer.ParameterMap, TEXT("ViewToVolumeClip"));
		VolumetricFogParameters.Bind(Initializer.ParameterMap);
	}

	FWriteToBoundingSphereVS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FSphere& BoundingSphere, int32 MinZValue)
	{
		SetShaderValue(RHICmdList, GetVertexShader(), MinZ, MinZValue);

		const FVector ViewSpaceBoundingSphereCenter = View.ViewMatrices.GetViewMatrix().TransformPosition(BoundingSphere.Center);
		SetShaderValue(RHICmdList, GetVertexShader(), ViewSpaceBoundingSphere, FVector4(ViewSpaceBoundingSphereCenter, BoundingSphere.W));

		const FMatrix ProjectionMatrix = View.ViewMatrices.ComputeProjectionNoAAMatrix();
		SetShaderValue(RHICmdList, GetVertexShader(), ViewToVolumeClip, ProjectionMatrix);

		VolumetricFogParameters.Set(RHICmdList, GetVertexShader(), View, NULL, NULL, NULL);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << MinZ;
		Ar << ViewSpaceBoundingSphere;
		Ar << ViewToVolumeClip;
		Ar << VolumetricFogParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter MinZ;
	FShaderParameter ViewSpaceBoundingSphere;
	FShaderParameter ViewToVolumeClip;
	FVolumetricFogIntegrationParameters VolumetricFogParameters;
};

IMPLEMENT_SHADER_TYPE(,FWriteToBoundingSphereVS,TEXT("VolumetricFog"),TEXT("WriteToBoundingSphereVS"),SF_Vertex);

/** Shader that adds direct lighting contribution from the given light to the current volume lighting cascade. */
template<bool bDynamicallyShadowed, bool bInverseSquared>
class TInjectShadowedLocalLightPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TInjectShadowedLocalLightPS,Global);
public:

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("DYNAMICALLY_SHADOWED"), (uint32)bDynamicallyShadowed);
		OutEnvironment.SetDefine(TEXT("INVERSE_SQUARED_FALLOFF"), (uint32)bInverseSquared);
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return DoesPlatformSupportVolumetricFog(Platform);
	}

	TInjectShadowedLocalLightPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		PhaseG.Bind(Initializer.ParameterMap, TEXT("PhaseG"));
		InverseSquaredLightDistanceBiasScale.Bind(Initializer.ParameterMap, TEXT("InverseSquaredLightDistanceBiasScale"));
		VolumetricFogParameters.Bind(Initializer.ParameterMap);
		VolumeShadowingParameters.Bind(Initializer.ParameterMap);
	}

	TInjectShadowedLocalLightPS() {}

	// @param InnerSplitIndex which CSM shadow map level, INDEX_NONE if no directional light
	// @param VolumeCascadeIndexValue which volume we render to
	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View, 
		const FLightSceneInfo* LightSceneInfo,
		const FExponentialHeightFogSceneInfo& FogInfo,
		const FProjectedShadowInfo* ShadowMap)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		
		SetDeferredLightParameters(RHICmdList, ShaderRHI, GetUniformBufferParameter<FDeferredLightUniformStruct>(), LightSceneInfo, View);

		VolumetricFogParameters.Set(RHICmdList, ShaderRHI, View, NULL, NULL, NULL);

		SetShaderValue(RHICmdList, ShaderRHI, PhaseG, FogInfo.VolumetricFogScatteringDistribution);
		SetShaderValue(RHICmdList, ShaderRHI, InverseSquaredLightDistanceBiasScale, GInverseSquaredLightDistanceBiasScale);

		VolumeShadowingParameters.Set(RHICmdList, ShaderRHI, View, LightSceneInfo, ShadowMap, 0, bDynamicallyShadowed);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PhaseG;
		Ar << InverseSquaredLightDistanceBiasScale;
		Ar << VolumetricFogParameters;
		Ar << VolumeShadowingParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter PhaseG;
	FShaderParameter InverseSquaredLightDistanceBiasScale;
	FVolumetricFogIntegrationParameters VolumetricFogParameters;
	FVolumeShadowingParameters VolumeShadowingParameters;
};

#define IMPLEMENT_LOCAL_LIGHT_INJECTION_PIXELSHADER_TYPE(bDynamicallyShadowed,bInverseSquared) \
	typedef TInjectShadowedLocalLightPS<bDynamicallyShadowed,bInverseSquared> TInjectShadowedLocalLightPS##bDynamicallyShadowed##bInverseSquared; \
	IMPLEMENT_SHADER_TYPE(template<>,TInjectShadowedLocalLightPS##bDynamicallyShadowed##bInverseSquared,TEXT("VolumetricFog"),TEXT("InjectShadowedLocalLightPS"),SF_Pixel);

IMPLEMENT_LOCAL_LIGHT_INJECTION_PIXELSHADER_TYPE(true, true);
IMPLEMENT_LOCAL_LIGHT_INJECTION_PIXELSHADER_TYPE(true, false);
IMPLEMENT_LOCAL_LIGHT_INJECTION_PIXELSHADER_TYPE(false, true);
IMPLEMENT_LOCAL_LIGHT_INJECTION_PIXELSHADER_TYPE(false, false);

FProjectedShadowInfo* GetShadowForInjectionIntoVolumetricFog(const FLightSceneProxy* LightProxy, FVisibleLightInfo& VisibleLightInfo)
{
	for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.ShadowsToProject.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.ShadowsToProject[ShadowIndex];

		if (ProjectedShadowInfo->bAllocated
			&& ProjectedShadowInfo->bWholeSceneShadow
			&& !ProjectedShadowInfo->bRayTracedDistanceField)
		{
			return ProjectedShadowInfo;
		}
	}

	return NULL;
}

bool LightNeedsSeparateInjectionIntoVolumetricFog(const FLightSceneInfo* LightSceneInfo, FVisibleLightInfo& VisibleLightInfo)
{
	const FLightSceneProxy* LightProxy = LightSceneInfo->Proxy;

	if (GVolumetricFogInjectShadowedLightsSeparately 
		&& (LightProxy->GetLightType() == LightType_Point || LightProxy->GetLightType() == LightType_Spot)
		&& !LightProxy->HasStaticLighting() 
		&& LightProxy->CastsDynamicShadow()
		&& LightProxy->CastsVolumetricShadow())
	{
		const FStaticShadowDepthMap* StaticShadowDepthMap = LightProxy->GetStaticShadowDepthMap();
		const bool bStaticallyShadowed = LightSceneInfo->IsPrecomputedLightingValid() && StaticShadowDepthMap && StaticShadowDepthMap->TextureRHI;

		return GetShadowForInjectionIntoVolumetricFog(LightProxy, VisibleLightInfo) != NULL || bStaticallyShadowed;
	}

	return false;
}

FIntPoint CalculateVolumetricFogBoundsForLight(const FSphere& LightBounds, const FViewInfo& View, FIntVector VolumetricFogGridSize, FVector GridZParams)
{
	FIntPoint VolumeZBounds;

	FVector ViewSpaceLightBoundsOrigin = View.ViewMatrices.GetViewMatrix().TransformPosition(LightBounds.Center);

	int32 FurthestSliceIndexUnclamped = ComputeZSliceFromDepth(ViewSpaceLightBoundsOrigin.Z + LightBounds.W, GridZParams);
	int32 ClosestSliceIndexUnclamped = ComputeZSliceFromDepth(ViewSpaceLightBoundsOrigin.Z - LightBounds.W, GridZParams);

	VolumeZBounds.X = FMath::Clamp(ClosestSliceIndexUnclamped, 0, VolumetricFogGridSize.Z - 1);
	VolumeZBounds.Y = FMath::Clamp(FurthestSliceIndexUnclamped, 0, VolumetricFogGridSize.Z - 1);

	return VolumeZBounds;
}

template<bool bDynamicallyShadowed, bool bInverseSquared>
void SetInjectShadowedLocalLightShaders(
	FRHICommandList& RHICmdList, 
	const FViewInfo& View, 
	const FLightSceneInfo* LightSceneInfo,
	const FSphere& LightBounds,
	const FExponentialHeightFogSceneInfo& FogInfo,
	const FProjectedShadowInfo* ProjectedShadowInfo,
	FIntVector VolumetricFogGridSize,
	int32 MinZ)
{
	TShaderMapRef<FWriteToBoundingSphereVS> VertexShader(View.ShaderMap);
	TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(View.ShaderMap);
	TShaderMapRef<TInjectShadowedLocalLightPS<bDynamicallyShadowed, bInverseSquared>> PixelShader(View.ShaderMap);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	// Accumulate the contribution of multiple lights
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI();

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.GeometryShaderRHI = GETSAFERHISHADER_GEOMETRY(*GeometryShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	PixelShader->SetParameters(RHICmdList, View, LightSceneInfo, FogInfo, ProjectedShadowInfo);
	VertexShader->SetParameters(RHICmdList, View, LightBounds, MinZ);

	if (GeometryShader.IsValid())
	{
		GeometryShader->SetParameters(RHICmdList, MinZ);
	}
}

void FDeferredShadingSceneRenderer::RenderLocalLightsForVolumetricFog(
	FRHICommandListImmediate& RHICmdList,
	FViewInfo& View,
	const FExponentialHeightFogSceneInfo& FogInfo,
	IPooledRenderTarget* VBufferA,
	FIntVector VolumetricFogGridSize, 
	FVector GridZParams,
	const FPooledRenderTargetDesc& VolumeDesc,
	TRefCountPtr<IPooledRenderTarget>& OutLocalShadowedLightScattering)
{
	TArray<const FLightSceneInfo*, SceneRenderingAllocator> LightsToInject;

	for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		const FLightSceneInfo* LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

		if (LightSceneInfo->ShouldRenderLightViewIndependent() 
			&& LightSceneInfo->ShouldRenderLight(View)
			&& LightNeedsSeparateInjectionIntoVolumetricFog(LightSceneInfo, VisibleLightInfos[LightSceneInfo->Id])
			&& LightSceneInfo->Proxy->GetVolumetricScatteringIntensity() > 0)
		{
			const FSphere LightBounds = LightSceneInfo->Proxy->GetBoundingSphere();

			if ((View.ViewMatrices.GetViewOrigin() - LightBounds.Center).SizeSquared() < (FogInfo.VolumetricFogDistance + LightBounds.W) * (FogInfo.VolumetricFogDistance + LightBounds.W))
			{
				LightsToInject.Add(LightSceneInfo);
			}
		}
	}

	if (LightsToInject.Num() > 0)
	{
		SCOPED_DRAW_EVENT(RHICmdList, ShadowedLights);

		GRenderTargetPool.FindFreeElement(RHICmdList, VolumeDesc, OutLocalShadowedLightScattering, TEXT("LocalShadowedLightScattering"));

		FRHIRenderTargetView ColorView(OutLocalShadowedLightScattering->GetRenderTargetItem().TargetableTexture, 0, -1, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore);
		FRHISetRenderTargetsInfo Info(1, &ColorView, FRHIDepthRenderTargetView());
		RHICmdList.SetRenderTargetsAndClear(Info);

		for (int32 LightIndex = 0; LightIndex < LightsToInject.Num(); LightIndex++)
		{
			const FLightSceneInfo* LightSceneInfo = LightsToInject[LightIndex];
			FProjectedShadowInfo* ProjectedShadowInfo = GetShadowForInjectionIntoVolumetricFog(LightSceneInfo->Proxy, VisibleLightInfos[LightSceneInfo->Id]);

			const bool bInverseSquared = LightSceneInfo->Proxy->IsInverseSquared();
			const bool bDynamicallyShadowed = ProjectedShadowInfo != NULL;
			const FSphere LightBounds = LightSceneInfo->Proxy->GetBoundingSphere();
			const FIntPoint VolumeZBounds = CalculateVolumetricFogBoundsForLight(LightBounds, View, VolumetricFogGridSize, GridZParams);

			if (VolumeZBounds.X < VolumeZBounds.Y)
			{
				if (bDynamicallyShadowed)
				{
					if (bInverseSquared)
					{
						SetInjectShadowedLocalLightShaders<true, true>(RHICmdList, View, LightSceneInfo, LightBounds, FogInfo, ProjectedShadowInfo, VolumetricFogGridSize, VolumeZBounds.X);
					}
					else
					{
						SetInjectShadowedLocalLightShaders<true, false>(RHICmdList, View, LightSceneInfo, LightBounds, FogInfo, ProjectedShadowInfo, VolumetricFogGridSize, VolumeZBounds.X);
					}
				}
				else
				{
					if (bInverseSquared)
					{
						SetInjectShadowedLocalLightShaders<false, true>(RHICmdList, View, LightSceneInfo, LightBounds, FogInfo, ProjectedShadowInfo, VolumetricFogGridSize, VolumeZBounds.X);
					}
					else
					{
						SetInjectShadowedLocalLightShaders<false, false>(RHICmdList, View, LightSceneInfo, LightBounds, FogInfo, ProjectedShadowInfo, VolumetricFogGridSize, VolumeZBounds.X);
					}
				}

				RHICmdList.SetStreamSource(0, GVolumeRasterizeVertexBuffer.VertexBufferRHI, sizeof(FScreenVertex), 0);
				const int32 NumInstances = VolumeZBounds.Y - VolumeZBounds.X;
				// Render a quad per slice affected by the given bounds
				RHICmdList.DrawPrimitive(PT_TriangleStrip, 0, 2, NumInstances);
			}
		}

		RHICmdList.CopyToResolveTarget(OutLocalShadowedLightScattering->GetRenderTargetItem().TargetableTexture, OutLocalShadowedLightScattering->GetRenderTargetItem().ShaderResourceTexture, true, FResolveParams());
	
		GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, OutLocalShadowedLightScattering);
	}
}

template<bool bTemporalReprojection, bool bDistanceFieldSkyOcclusion>
class TVolumetricFogLightScatteringCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TVolumetricFogLightScatteringCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return DoesPlatformSupportVolumetricFog(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), VolumetricFogGridInjectionGroupSize);
		FVolumetricFogIntegrationParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
		FForwardLightingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_TEMPORAL_REPROJECTION"), bTemporalReprojection);
		OutEnvironment.SetDefine(TEXT("DISTANCE_FIELD_SKY_OCCLUSION"), bDistanceFieldSkyOcclusion);
	}

	TVolumetricFogLightScatteringCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		HistoryWeight.Bind(Initializer.ParameterMap, TEXT("HistoryWeight"));
		LocalShadowedLightScattering.Bind(Initializer.ParameterMap, TEXT("LocalShadowedLightScattering"));
		LightScatteringHistory.Bind(Initializer.ParameterMap, TEXT("LightScatteringHistory"));
		LightScatteringHistorySampler.Bind(Initializer.ParameterMap, TEXT("LightScatteringHistorySampler"));
		VolumetricFogParameters.Bind(Initializer.ParameterMap);
		ForwardLightingParameters.Bind(Initializer.ParameterMap);
		DirectionalLightFunctionWorldToShadow.Bind(Initializer.ParameterMap, TEXT("DirectionalLightFunctionWorldToShadow"));
		LightFunctionTexture.Bind(Initializer.ParameterMap, TEXT("LightFunctionTexture"));
		LightFunctionSampler.Bind(Initializer.ParameterMap, TEXT("LightFunctionSampler"));
		SkyLightVolumetricScatteringIntensity.Bind(Initializer.ParameterMap, TEXT("SkyLightVolumetricScatteringIntensity"));
		SkySH.Bind(Initializer.ParameterMap, TEXT("SkySH"));
		PhaseG.Bind(Initializer.ParameterMap, TEXT("PhaseG"));
		InverseSquaredLightDistanceBiasScale.Bind(Initializer.ParameterMap, TEXT("InverseSquaredLightDistanceBiasScale"));
		UseHeightFogColors.Bind(Initializer.ParameterMap, TEXT("UseHeightFogColors"));
		UseDirectionalLightShadowing.Bind(Initializer.ParameterMap, TEXT("UseDirectionalLightShadowing"));
		AOParameters.Bind(Initializer.ParameterMap);
		GlobalDistanceFieldParameters.Bind(Initializer.ParameterMap);
		HeightFogParameters.Bind(Initializer.ParameterMap);
	}

	TVolumetricFogLightScatteringCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View, 
		const FExponentialHeightFogSceneInfo& FogInfo,
		IPooledRenderTarget* VBufferA,
		IPooledRenderTarget* VBufferB,
		IPooledRenderTarget* LocalShadowedLightScatteringTarget,
		IPooledRenderTarget* LightScatteringRenderTarget,
		FTextureRHIParamRef LightScatteringHistoryTexture,
		bool bUseDirectionalLightShadowing,
		const FMatrix& DirectionalLightFunctionWorldToShadowValue,
		TRefCountPtr<IPooledRenderTarget>& LightFunctionTextureValue)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		SetShaderValue(RHICmdList, ShaderRHI, HistoryWeight, GVolumetricFogHistoryWeight);

		FTextureRHIParamRef LocalShadowedLightScatteringTexture = GBlackVolumeTexture->TextureRHI;

		if (LocalShadowedLightScatteringTarget)
		{
			LocalShadowedLightScatteringTexture = LocalShadowedLightScatteringTarget->GetRenderTargetItem().ShaderResourceTexture;
		}

		SetTextureParameter(RHICmdList, ShaderRHI, LocalShadowedLightScattering, LocalShadowedLightScatteringTexture);

		if (!LightScatteringHistoryTexture)
		{
			LightScatteringHistoryTexture = GBlackVolumeTexture->TextureRHI;
		}

		SetTextureParameter(
			RHICmdList, 
			ShaderRHI, 
			LightScatteringHistory, 
			LightScatteringHistorySampler, 
			TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 
			LightScatteringHistoryTexture);

		VolumetricFogParameters.Set(RHICmdList, ShaderRHI, View, VBufferA, VBufferB, LightScatteringRenderTarget);
		ForwardLightingParameters.Set(RHICmdList, ShaderRHI, View);

		SetShaderValue(RHICmdList, ShaderRHI, DirectionalLightFunctionWorldToShadow, DirectionalLightFunctionWorldToShadowValue);

		FTextureRHIParamRef LightFunctionRHITexture = GWhiteTexture->TextureRHI;

		if (LightFunctionTextureValue)
		{
			LightFunctionRHITexture = LightFunctionTextureValue->GetRenderTargetItem().ShaderResourceTexture;
		}

		SetTextureParameter(RHICmdList, ShaderRHI, LightFunctionTexture, LightFunctionSampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), LightFunctionRHITexture);

		FScene* Scene = (FScene*)View.Family->Scene;
		FDistanceFieldAOParameters AOParameterData(Scene->DefaultMaxDistanceFieldOcclusionDistance);

		if (Scene->SkyLight
			// Skylights with static lighting had their diffuse contribution baked into lightmaps
			&& !Scene->SkyLight->bHasStaticLighting
			&& View.Family->EngineShowFlags.SkyLighting)
		{
			SetShaderValue(RHICmdList, ShaderRHI, SkyLightVolumetricScatteringIntensity, Scene->SkyLight->VolumetricScatteringIntensity);

			const FSHVectorRGB3& SkyIrradiance = Scene->SkyLight->IrradianceEnvironmentMap;
			SetShaderValue(RHICmdList, ShaderRHI, SkySH, (FVector4&)SkyIrradiance.R.V, 0);
			SetShaderValue(RHICmdList, ShaderRHI, SkySH, (FVector4&)SkyIrradiance.G.V, 1);
			SetShaderValue(RHICmdList, ShaderRHI, SkySH, (FVector4&)SkyIrradiance.B.V, 2);

			AOParameterData = FDistanceFieldAOParameters(Scene->SkyLight->OcclusionMaxDistance, Scene->SkyLight->Contrast);
		}
		else
		{
			SetShaderValue(RHICmdList, ShaderRHI, SkyLightVolumetricScatteringIntensity, 0.0f);
			SetShaderValue(RHICmdList, ShaderRHI, SkySH, FVector4(0, 0, 0, 0), 0);
			SetShaderValue(RHICmdList, ShaderRHI, SkySH, FVector4(0, 0, 0, 0), 1);
			SetShaderValue(RHICmdList, ShaderRHI, SkySH, FVector4(0, 0, 0, 0), 2);
		}

		SetShaderValue(RHICmdList, ShaderRHI, PhaseG, FogInfo.VolumetricFogScatteringDistribution);
		SetShaderValue(RHICmdList, ShaderRHI, InverseSquaredLightDistanceBiasScale, GInverseSquaredLightDistanceBiasScale);
		SetShaderValue(RHICmdList, ShaderRHI, UseHeightFogColors, FogInfo.bOverrideLightColorsWithFogInscatteringColors ? 1.0f : 0.0f);
		SetShaderValue(RHICmdList, ShaderRHI, UseDirectionalLightShadowing, bUseDirectionalLightShadowing ? 1.0f : 0.0f);

		AOParameters.Set(RHICmdList, ShaderRHI, AOParameterData);
		GlobalDistanceFieldParameters.Set(RHICmdList, ShaderRHI, View.GlobalDistanceFieldInfo.ParameterData);
		HeightFogParameters.Set(RHICmdList, ShaderRHI, &View);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, IPooledRenderTarget* LightScatteringRenderTarget)
	{
		VolumetricFogParameters.UnsetParameters(RHICmdList, GetComputeShader(), View, NULL, NULL, LightScatteringRenderTarget, true);
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << HistoryWeight;
		Ar << LocalShadowedLightScattering;
		Ar << LightScatteringHistory;
		Ar << LightScatteringHistorySampler;
		Ar << VolumetricFogParameters;
		Ar << ForwardLightingParameters;
		Ar << DirectionalLightFunctionWorldToShadow;
		Ar << LightFunctionTexture;
		Ar << LightFunctionSampler;
		Ar << SkyLightVolumetricScatteringIntensity;
		Ar << SkySH;
		Ar << PhaseG;
		Ar << InverseSquaredLightDistanceBiasScale;
		Ar << UseHeightFogColors;
		Ar << UseDirectionalLightShadowing;
		Ar << AOParameters;
		Ar << GlobalDistanceFieldParameters;
		Ar << HeightFogParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderParameter HistoryWeight;
	FShaderResourceParameter LocalShadowedLightScattering;
	FShaderResourceParameter LightScatteringHistory;
	FShaderResourceParameter LightScatteringHistorySampler;
	FVolumetricFogIntegrationParameters VolumetricFogParameters;
	FForwardLightingParameters ForwardLightingParameters;
	FShaderParameter DirectionalLightFunctionWorldToShadow;
	FShaderResourceParameter LightFunctionTexture;
	FShaderResourceParameter LightFunctionSampler;
	FShaderParameter SkyLightVolumetricScatteringIntensity;
	FShaderParameter SkySH;
	FShaderParameter PhaseG;
	FShaderParameter InverseSquaredLightDistanceBiasScale;
	FShaderParameter UseHeightFogColors;
	FShaderParameter UseDirectionalLightShadowing;
	FAOParameters AOParameters;
	FGlobalDistanceFieldParameters GlobalDistanceFieldParameters;
	FExponentialHeightFogShaderParameters HeightFogParameters;
};

#define IMPLEMENT_VOLUMETRICFOG_LIGHT_SCATTERING_CS_TYPE(bTemporalReprojection, bDistanceFieldSkyOcclusion) \
	typedef TVolumetricFogLightScatteringCS<bTemporalReprojection, bDistanceFieldSkyOcclusion> TVolumetricFogLightScatteringCS##bTemporalReprojection##bDistanceFieldSkyOcclusion; \
	IMPLEMENT_SHADER_TYPE(template<>,TVolumetricFogLightScatteringCS##bTemporalReprojection##bDistanceFieldSkyOcclusion,TEXT("VolumetricFog"),TEXT("LightScatteringCS"),SF_Compute);

IMPLEMENT_VOLUMETRICFOG_LIGHT_SCATTERING_CS_TYPE(true, true)
IMPLEMENT_VOLUMETRICFOG_LIGHT_SCATTERING_CS_TYPE(false, true)
IMPLEMENT_VOLUMETRICFOG_LIGHT_SCATTERING_CS_TYPE(true, false)
IMPLEMENT_VOLUMETRICFOG_LIGHT_SCATTERING_CS_TYPE(false, false)

uint32 VolumetricFogIntegrationGroupSize = 8;

class FVolumetricFogFinalIntegrationCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVolumetricFogFinalIntegrationCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return DoesPlatformSupportVolumetricFog(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), VolumetricFogIntegrationGroupSize);
		FVolumetricFogIntegrationParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FVolumetricFogFinalIntegrationCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		VolumetricFogParameters.Bind(Initializer.ParameterMap);
	}

	FVolumetricFogFinalIntegrationCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, IPooledRenderTarget* LightScatteringRenderTarget)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		VolumetricFogParameters.Set(RHICmdList, ShaderRHI, View, NULL, NULL, LightScatteringRenderTarget);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		VolumetricFogParameters.UnsetParameters(RHICmdList, GetComputeShader(), View, NULL, NULL, NULL, true);
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << VolumetricFogParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FVolumetricFogIntegrationParameters VolumetricFogParameters;
};

IMPLEMENT_SHADER_TYPE(,FVolumetricFogFinalIntegrationCS,TEXT("VolumetricFog"),TEXT("FinalIntegrationCS"),SF_Compute);

bool ShouldRenderVolumetricFog(const FScene* Scene, const FSceneViewFamily& ViewFamily)
{
	return ShouldRenderFog(ViewFamily)
		&& Scene
		&& Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM5 
		&& DoesPlatformSupportVolumetricFog(Scene->GetShaderPlatform())
		&& GVolumetricFog
		&& ViewFamily.EngineShowFlags.VolumetricFog
		&& Scene->ExponentialFogs.Num() > 0
		&& Scene->ExponentialFogs[0].bEnableVolumetricFog
		&& Scene->ExponentialFogs[0].VolumetricFogDistance > 0;
}

FVector GetVolumetricFogGridZParams(float NearPlane, float FarPlane, int32 GridSizeZ)
{
	// S = distribution scale
	// B, O are solved for given the z distances of the first+last slice, and the # of slices.
	//
	// slice = log2(z*B + O) * S

	// Don't spend lots of resolution right in front of the near plane
	double NearOffset = .095 * 100;
	// Space out the slices so they aren't all clustered at the near plane
	double S = GVolumetricFogDepthDistributionScale;

	double N = NearPlane + NearOffset;
	double F = FarPlane;

	double O = (F - N * exp2((GridSizeZ - 1) / S)) / (F - N);
	double B = (1 - O) / N;

	double O2 = (exp2((GridSizeZ - 1) / S) - F / N) / (-F / N + 1);

	float FloatN = (float)N;
	float FloatF = (float)F;
	float FloatB = (float)B;
	float FloatO = (float)O;
	float FloatS = (float)S;

	float NSlice = log2(FloatN*FloatB + FloatO) * FloatS;
	float NearPlaneSlice = log2(NearPlane*FloatB + FloatO) * FloatS;
	float FSlice = log2(FloatF*FloatB + FloatO) * FloatS;
	// y = log2(z*B + O) * S
	// f(N) = 0 = log2(N*B + O) * S
	// 1 = N*B + O
	// O = 1 - N*B
	// B = (1 - O) / N

	// f(F) = GLightGridSizeZ - 1 = log2(F*B + O) * S
	// exp2((GLightGridSizeZ - 1) / S) = F*B + O
	// exp2((GLightGridSizeZ - 1) / S) = F * (1 - O) / N + O
	// exp2((GLightGridSizeZ - 1) / S) = F / N - F / N * O + O
	// exp2((GLightGridSizeZ - 1) / S) = F / N + (-F / N + 1) * O
	// O = (exp2((GLightGridSizeZ - 1) / S) - F / N) / (-F / N + 1)

	return FVector(B, O, S);
}

FIntVector GetVolumetricFogGridSize(FIntPoint ViewRectSize)
{
	extern int32 GLightGridSizeZ;
	const FIntPoint VolumetricFogGridSizeXY = FIntPoint::DivideAndRoundUp(ViewRectSize, GVolumetricFogGridPixelSize);
	return FIntVector(VolumetricFogGridSizeXY.X, VolumetricFogGridSizeXY.Y, GVolumetricFogGridSizeZ);
}

void FViewInfo::SetupVolumetricFogUniformBufferParameters(FViewUniformShaderParameters& ViewUniformShaderParameters) const
{
	const FScene* Scene = (const FScene*)Family->Scene;

	if (ShouldRenderVolumetricFog(Scene, *Family))
	{
		const FExponentialHeightFogSceneInfo& FogInfo = Scene->ExponentialFogs[0];

		const FIntVector VolumetricFogGridSize = GetVolumetricFogGridSize(ViewRect.Size());
			
		ViewUniformShaderParameters.VolumetricFogInvGridSize = FVector(1.0f / VolumetricFogGridSize.X, 1.0f / VolumetricFogGridSize.Y, 1.0f / VolumetricFogGridSize.Z);

		const FVector ZParams = GetVolumetricFogGridZParams(NearClippingDistance, FogInfo.VolumetricFogDistance, VolumetricFogGridSize.Z);
		ViewUniformShaderParameters.VolumetricFogGridZParams = ZParams;

		ViewUniformShaderParameters.VolumetricFogSVPosToVolumeUV = FVector2D(1.0f, 1.0f) / (FVector2D(VolumetricFogGridSize.X, VolumetricFogGridSize.Y) * GVolumetricFogGridPixelSize);
		ViewUniformShaderParameters.VolumetricFogMaxDistance = FogInfo.VolumetricFogDistance;
	}
	else
	{
		ViewUniformShaderParameters.VolumetricFogInvGridSize = FVector::ZeroVector;
		ViewUniformShaderParameters.VolumetricFogGridZParams = FVector::ZeroVector;
		ViewUniformShaderParameters.VolumetricFogSVPosToVolumeUV = FVector2D(0, 0);
		ViewUniformShaderParameters.VolumetricFogMaxDistance = 0;
	}
}

void FDeferredShadingSceneRenderer::SetupVolumetricFog()
{
	if (ShouldRenderVolumetricFog(Scene, ViewFamily))
	{
		const FExponentialHeightFogSceneInfo& FogInfo = Scene->ExponentialFogs[0];

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];

			const FIntVector VolumetricFogGridSize = GetVolumetricFogGridSize(View.ViewRect.Size());
			
			FVolumetricFogGlobalData GlobalData;
			GlobalData.GridSizeInt = VolumetricFogGridSize;
			GlobalData.GridSize = FVector(VolumetricFogGridSize);

			FVector ZParams = GetVolumetricFogGridZParams(View.NearClippingDistance, FogInfo.VolumetricFogDistance, VolumetricFogGridSize.Z);
			GlobalData.GridZParams = ZParams;

			GlobalData.SVPosToVolumeUV = FVector2D(1.0f, 1.0f) / (FVector2D(GlobalData.GridSize) * GVolumetricFogGridPixelSize);
			GlobalData.FogGridToPixelXY = FIntPoint(GVolumetricFogGridPixelSize, GVolumetricFogGridPixelSize);
			GlobalData.MaxDistance = FogInfo.VolumetricFogDistance;

			GlobalData.HeightFogInscatteringColor = View.ExponentialFogColor;

			GlobalData.HeightFogDirectionalLightInscatteringColor = FVector::ZeroVector;

			if (View.bUseDirectionalInscattering && !View.FogInscatteringColorCubemap)
			{
				GlobalData.HeightFogDirectionalLightInscatteringColor = FVector(View.DirectionalInscatteringColor);
			}

			View.VolumetricFogResources.VolumetricFogGlobalData = TUniformBufferRef<FVolumetricFogGlobalData>::CreateUniformBufferImmediate(GlobalData, UniformBuffer_SingleFrame);
		}
	}
	else
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];

			if (View.ViewState)
			{
				View.ViewState->LightScatteringHistory = NULL;
			}
		}
	}
}

void FDeferredShadingSceneRenderer::ComputeVolumetricFog(FRHICommandListImmediate& RHICmdList)
{
	if (ShouldRenderVolumetricFog(Scene, ViewFamily))
	{
		const FExponentialHeightFogSceneInfo& FogInfo = Scene->ExponentialFogs[0];

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];

			const FIntVector VolumetricFogGridSize = GetVolumetricFogGridSize(View.ViewRect.Size());
			const FVector GridZParams = GetVolumetricFogGridZParams(View.NearClippingDistance, FogInfo.VolumetricFogDistance, VolumetricFogGridSize.Z);

			SCOPED_DRAW_EVENT(RHICmdList, VolumetricFog);

			FMatrix LightFunctionWorldToShadow;
			TRefCountPtr<IPooledRenderTarget> LightFunctionTexture;
			bool bUseDirectionalLightShadowing;

			RenderLightFunctionForVolumetricFog(
				RHICmdList,
				View,
				VolumetricFogGridSize,
				FogInfo.VolumetricFogDistance,
				LightFunctionWorldToShadow,
				LightFunctionTexture,
				bUseDirectionalLightShadowing);

			TRefCountPtr<IPooledRenderTarget> VBufferA;
			TRefCountPtr<IPooledRenderTarget> VBufferB;

			const uint32 Flags = TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV | TexCreate_ReduceMemoryWithTilingMode;
			FPooledRenderTargetDesc VolumeDesc(FPooledRenderTargetDesc::CreateVolumeDesc(VolumetricFogGridSize.X, VolumetricFogGridSize.Y, VolumetricFogGridSize.Z, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_None, Flags, false));
			GRenderTargetPool.FindFreeElement(RHICmdList, VolumeDesc, VBufferA, TEXT("VBufferA"));
			GRenderTargetPool.FindFreeElement(RHICmdList, VolumeDesc, VBufferB, TEXT("VBufferB"));

			// Unbind render targets, the shadow depth target may still be bound
			SetRenderTarget(RHICmdList, NULL, NULL);

			{
				const FIntVector NumGroups = FIntVector::DivideAndRoundUp(VolumetricFogGridSize, VolumetricFogGridInjectionGroupSize);

				{
					SCOPED_DRAW_EVENT(RHICmdList, InitializeVolumeAttributes);
					TShaderMapRef<FVolumetricFogMaterialSetupCS> ComputeShader(View.ShaderMap);
					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, View, FogInfo, VBufferA.GetReference(), VBufferB.GetReference());
					DispatchComputeShader(RHICmdList, *ComputeShader, NumGroups.X, NumGroups.Y, NumGroups.Z);
					ComputeShader->UnsetParameters(RHICmdList, View, VBufferA.GetReference(), VBufferB.GetReference());
				}

				VoxelizeFogVolumePrimitives(
					RHICmdList, 
					View, 
					VolumetricFogGridSize, 
					GridZParams,
					FogInfo.VolumetricFogDistance, 
					VBufferA.GetReference(), 
					VBufferB.GetReference());

				FTextureRHIParamRef VoxelizeRenderTargets[2] =
				{
					VBufferA->GetRenderTargetItem().TargetableTexture,
					VBufferB->GetRenderTargetItem().TargetableTexture
				};

				RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, VoxelizeRenderTargets, ARRAY_COUNT(VoxelizeRenderTargets));

				GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, VBufferA);
				GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, VBufferB);
			}

			TRefCountPtr<IPooledRenderTarget> LocalShadowedLightScattering = NULL;
			RenderLocalLightsForVolumetricFog(RHICmdList, View, FogInfo, VBufferA.GetReference(), VolumetricFogGridSize, GridZParams, VolumeDesc, LocalShadowedLightScattering);

			TRefCountPtr<IPooledRenderTarget> LightScattering;
			GRenderTargetPool.FindFreeElement(RHICmdList, VolumeDesc, LightScattering, TEXT("LightScattering"));

			SetRenderTarget(RHICmdList, NULL, NULL);

			{
				const FIntVector NumGroups = FIntVector::DivideAndRoundUp(VolumetricFogGridSize, VolumetricFogGridInjectionGroupSize);

				const bool bUseTemporalReprojection = 
					GVolumetricFogTemporalReprojection 
					&& View.ViewState
					&& !View.bCameraCut
					&& !View.bPrevTransformsReset;

				const bool bUseGlobalDistanceField = UseGlobalDistanceField() && Scene->DistanceFieldSceneData.NumObjectsInBuffer > 0;

				const bool bUseDistanceFieldSkyOcclusion =
					ViewFamily.EngineShowFlags.AmbientOcclusion
					&& Scene->SkyLight
					&& Scene->SkyLight->bCastShadows 
					&& Scene->SkyLight->bCastVolumetricShadow
					&& ShouldRenderDistanceFieldAO() 
					&& SupportsDistanceFieldAO(View.GetFeatureLevel(), View.GetShaderPlatform())
					&& bUseGlobalDistanceField
					&& Views.Num() == 1
					&& View.IsPerspectiveProjection();

				SCOPED_DRAW_EVENTF(RHICmdList, LightScattering, TEXT("LightScattering %dx%dx%d %s %s"), 
					VolumetricFogGridSize.X, 
					VolumetricFogGridSize.Y, 
					VolumetricFogGridSize.Z,
					bUseDistanceFieldSkyOcclusion ? TEXT("DFAO") : TEXT(""),
					LightFunctionTexture ? TEXT("LF") : TEXT(""));

				if (bUseTemporalReprojection && View.ViewState->LightScatteringHistory)
				{
					if (bUseDistanceFieldSkyOcclusion)
					{
						TShaderMapRef<TVolumetricFogLightScatteringCS<true, true> > ComputeShader(View.ShaderMap);
						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, View, FogInfo, VBufferA.GetReference(), VBufferB.GetReference(), LocalShadowedLightScattering.GetReference(), LightScattering.GetReference(), View.ViewState->LightScatteringHistory->GetRenderTargetItem().ShaderResourceTexture, bUseDirectionalLightShadowing, LightFunctionWorldToShadow, LightFunctionTexture);
						DispatchComputeShader(RHICmdList, *ComputeShader, NumGroups.X, NumGroups.Y, NumGroups.Z);
						ComputeShader->UnsetParameters(RHICmdList, View, LightScattering.GetReference());
					}
					else
					{
						TShaderMapRef<TVolumetricFogLightScatteringCS<true, false> > ComputeShader(View.ShaderMap);
						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, View, FogInfo, VBufferA.GetReference(), VBufferB.GetReference(), LocalShadowedLightScattering.GetReference(), LightScattering.GetReference(), View.ViewState->LightScatteringHistory->GetRenderTargetItem().ShaderResourceTexture, bUseDirectionalLightShadowing, LightFunctionWorldToShadow, LightFunctionTexture);
						DispatchComputeShader(RHICmdList, *ComputeShader, NumGroups.X, NumGroups.Y, NumGroups.Z);
						ComputeShader->UnsetParameters(RHICmdList, View, LightScattering.GetReference());
					}
				}
				else
				{
					if (bUseDistanceFieldSkyOcclusion)
					{
						TShaderMapRef<TVolumetricFogLightScatteringCS<false, true> > ComputeShader(View.ShaderMap);
						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, View, FogInfo, VBufferA.GetReference(), VBufferB.GetReference(), LocalShadowedLightScattering.GetReference(), LightScattering.GetReference(), NULL, bUseDirectionalLightShadowing, LightFunctionWorldToShadow, LightFunctionTexture);
						DispatchComputeShader(RHICmdList, *ComputeShader, NumGroups.X, NumGroups.Y, NumGroups.Z);
						ComputeShader->UnsetParameters(RHICmdList, View, LightScattering.GetReference());
					}
					else
					{
						TShaderMapRef<TVolumetricFogLightScatteringCS<false, false> > ComputeShader(View.ShaderMap);
						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, View, FogInfo, VBufferA.GetReference(), VBufferB.GetReference(), LocalShadowedLightScattering.GetReference(), LightScattering.GetReference(), NULL, bUseDirectionalLightShadowing, LightFunctionWorldToShadow, LightFunctionTexture);
						DispatchComputeShader(RHICmdList, *ComputeShader, NumGroups.X, NumGroups.Y, NumGroups.Z);
						ComputeShader->UnsetParameters(RHICmdList, View, LightScattering.GetReference());
					}
				}

				GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, LightScattering);

				if (bUseTemporalReprojection)
				{
					View.ViewState->LightScatteringHistory = LightScattering;
				}
				else if (View.ViewState)
				{
					View.ViewState->LightScatteringHistory = NULL;
				}
			}

			VBufferA = NULL;
			VBufferB = NULL;

			GRenderTargetPool.FindFreeElement(RHICmdList, VolumeDesc, View.VolumetricFogResources.IntegratedLightScattering, TEXT("IntegratedLightScattering"));

			{
				SCOPED_DRAW_EVENT(RHICmdList, FinalIntegration);

				const FIntVector NumGroups = FIntVector::DivideAndRoundUp(VolumetricFogGridSize, VolumetricFogIntegrationGroupSize);
				TShaderMapRef<FVolumetricFogFinalIntegrationCS> ComputeShader(View.ShaderMap);
				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, View, LightScattering.GetReference());
				DispatchComputeShader(RHICmdList, *ComputeShader, NumGroups.X, NumGroups.Y, 1);
				ComputeShader->UnsetParameters(RHICmdList, View);
			}

			GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, View.VolumetricFogResources.IntegratedLightScattering);
		}
	}
}
