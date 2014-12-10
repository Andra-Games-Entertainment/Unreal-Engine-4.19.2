// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	SimpleElementShaders.h: Definitions for simple element shaders.
==============================================================================*/

#include "EnginePrivate.h"
#include "ShaderParameterUtils.h"
#include "SimpleElementShaders.h"

/*------------------------------------------------------------------------------
	Simple element vertex shader.
------------------------------------------------------------------------------*/

FSimpleElementVS::FSimpleElementVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	FGlobalShader(Initializer)
{
	Transform.Bind(Initializer.ParameterMap,TEXT("Transform"), SPF_Mandatory);
	SwitchVerticalAxis.Bind(Initializer.ParameterMap,TEXT("SwitchVerticalAxis"), SPF_Optional);
}

void FSimpleElementVS::SetParameters(FRHICommandList& RHICmdList, const FMatrix& TransformValue, bool bSwitchVerticalAxis)
{
	SetShaderValue(RHICmdList, GetVertexShader(),Transform,TransformValue);
	SetShaderValue(RHICmdList, GetVertexShader(),SwitchVerticalAxis, bSwitchVerticalAxis ? -1.0f : 1.0f);
}

bool FSimpleElementVS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
	Ar << Transform << SwitchVerticalAxis;
	return bShaderHasOutdatedParameters;
}

void FSimpleElementVS::ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("ALLOW_SWITCH_VERTICALAXIS"), (Platform != SP_METAL));
}

/*------------------------------------------------------------------------------
	Simple element pixel shaders.
------------------------------------------------------------------------------*/

FSimpleElementPS::FSimpleElementPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
	FGlobalShader(Initializer)
{
	InTexture.Bind(Initializer.ParameterMap,TEXT("InTexture"));
	InTextureSampler.Bind(Initializer.ParameterMap,TEXT("InTextureSampler"));
	TextureComponentReplicate.Bind(Initializer.ParameterMap,TEXT("TextureComponentReplicate"));
	TextureComponentReplicateAlpha.Bind(Initializer.ParameterMap,TEXT("TextureComponentReplicateAlpha"));
	SceneDepthTextureNonMS.Bind(Initializer.ParameterMap,TEXT("SceneDepthTextureNonMS"));
	EditorCompositeDepthTestParameter.Bind(Initializer.ParameterMap,TEXT("bEnableEditorPrimitiveDepthTest"));
	ScreenToPixel.Bind(Initializer.ParameterMap,TEXT("ScreenToPixel"));
}

void FSimpleElementPS::SetEditorCompositingParameters(FRHICommandList& RHICmdList, const FSceneView* View, FTexture2DRHIRef DepthTexture )
{
	if( View )
	{
		FGlobalShader::SetParameters(RHICmdList, GetPixelShader(), *View );

		FIntRect DestRect = View->ViewRect;
		FIntPoint ViewportOffset = DestRect.Min;
		FIntPoint ViewportExtent = DestRect.Size();

		FVector4 ScreenPosToPixelValue(
			ViewportExtent.X * 0.5f,
			-ViewportExtent.Y * 0.5f, 
			ViewportExtent.X * 0.5f - 0.5f + ViewportOffset.X,
			ViewportExtent.Y * 0.5f - 0.5f + ViewportOffset.Y);

		SetShaderValue(RHICmdList, GetPixelShader(), ScreenToPixel, ScreenPosToPixelValue);

		SetShaderValue(RHICmdList, GetPixelShader(),EditorCompositeDepthTestParameter,IsValidRef(DepthTexture) );
	}
	else
	{
		// Unset the view uniform buffer since we don't have a view
		SetUniformBufferParameter(RHICmdList, GetPixelShader(), GetUniformBufferParameter<FViewUniformShaderParameters>(), NULL);
		SetShaderValue(RHICmdList, GetPixelShader(),EditorCompositeDepthTestParameter,false );
	}

	// Bind the zbuffer as a texture if depth textures are supported
	SetTextureParameter(RHICmdList, GetPixelShader(), SceneDepthTextureNonMS, IsValidRef(DepthTexture) ? (FTextureRHIRef)DepthTexture : GWhiteTexture->TextureRHI);
}

void FSimpleElementPS::SetParameters(FRHICommandList& RHICmdList, const FTexture* TextureValue)
{
	SetTextureParameter(RHICmdList, GetPixelShader(),InTexture,InTextureSampler,TextureValue);
	
	SetShaderValue(RHICmdList, GetPixelShader(),TextureComponentReplicate,TextureValue->bGreyScaleFormat ? FLinearColor(1,0,0,0) : FLinearColor(0,0,0,0));
	SetShaderValue(RHICmdList, GetPixelShader(),TextureComponentReplicateAlpha,TextureValue->bGreyScaleFormat ? FLinearColor(1,0,0,0) : FLinearColor(0,0,0,1));
}

bool FSimpleElementPS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
	Ar << InTexture;
	Ar << InTextureSampler;
	Ar << TextureComponentReplicate;
	Ar << TextureComponentReplicateAlpha;
	Ar << SceneDepthTextureNonMS;
	Ar << EditorCompositeDepthTestParameter;
	Ar << ScreenToPixel;
	return bShaderHasOutdatedParameters;
}

FSimpleElementAlphaOnlyPS::FSimpleElementAlphaOnlyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
	FSimpleElementPS(Initializer)
{
}

FSimpleElementGammaBasePS::FSimpleElementGammaBasePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
	FSimpleElementPS(Initializer)
{
	Gamma.Bind(Initializer.ParameterMap,TEXT("Gamma"));
}

void FSimpleElementGammaBasePS::SetParameters(FRHICommandList& RHICmdList, const FTexture* Texture, float GammaValue, ESimpleElementBlendMode BlendMode)
{
	FSimpleElementPS::SetParameters(RHICmdList, Texture);
	SetShaderValue(RHICmdList, GetPixelShader(),Gamma,GammaValue);
}

bool FSimpleElementGammaBasePS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FSimpleElementPS::Serialize(Ar);
	Ar << Gamma;
	return bShaderHasOutdatedParameters;
}

FSimpleElementMaskedGammaBasePS::FSimpleElementMaskedGammaBasePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
	FSimpleElementGammaBasePS(Initializer)
{
	ClipRef.Bind(Initializer.ParameterMap,TEXT("ClipRef"), SPF_Mandatory);
}

void FSimpleElementMaskedGammaBasePS::SetParameters(FRHICommandList& RHICmdList, const FTexture* Texture, float Gamma, float ClipRefValue, ESimpleElementBlendMode BlendMode)
{
	FSimpleElementGammaBasePS::SetParameters(RHICmdList, Texture,Gamma,BlendMode);
	SetShaderValue(RHICmdList, GetPixelShader(),ClipRef,ClipRefValue);
}

bool FSimpleElementMaskedGammaBasePS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FSimpleElementGammaBasePS::Serialize(Ar);
	Ar << ClipRef;
	return bShaderHasOutdatedParameters;
}

/**
* Constructor
*
* @param Initializer - shader initialization container
*/
FSimpleElementDistanceFieldGammaPS::FSimpleElementDistanceFieldGammaPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FSimpleElementMaskedGammaBasePS(Initializer)
{
	SmoothWidth.Bind(Initializer.ParameterMap,TEXT("SmoothWidth"));
	EnableShadow.Bind(Initializer.ParameterMap,TEXT("EnableShadow"));
	ShadowDirection.Bind(Initializer.ParameterMap,TEXT("ShadowDirection"));
	ShadowColor.Bind(Initializer.ParameterMap,TEXT("ShadowColor"));
	ShadowSmoothWidth.Bind(Initializer.ParameterMap,TEXT("ShadowSmoothWidth"));
	EnableGlow.Bind(Initializer.ParameterMap,TEXT("EnableGlow"));
	GlowColor.Bind(Initializer.ParameterMap,TEXT("GlowColor"));
	GlowOuterRadius.Bind(Initializer.ParameterMap,TEXT("GlowOuterRadius"));
	GlowInnerRadius.Bind(Initializer.ParameterMap,TEXT("GlowInnerRadius"));
}

/**
* Sets all the constant parameters for this shader
*
* @param Texture - 2d tile texture
* @param Gamma - if gamma != 1.0 then a pow(color,Gamma) is applied
* @param ClipRef - reference value to compare with alpha for killing pixels
* @param SmoothWidth - The width to smooth the edge the texture
* @param EnableShadow - Toggles drop shadow rendering
* @param ShadowDirection - 2D vector specifying the direction of shadow
* @param ShadowColor - Color of the shadowed pixels
* @param ShadowSmoothWidth - The width to smooth the edge the shadow of the texture
* @param BlendMode - current batched element blend mode being rendered
*/
void FSimpleElementDistanceFieldGammaPS::SetParameters(
	FRHICommandList& RHICmdList, 
	const FTexture* Texture,
	float Gamma,
	float ClipRef,
	float SmoothWidthValue,
	bool EnableShadowValue,
	const FVector2D& ShadowDirectionValue,
	const FLinearColor& ShadowColorValue,
	float ShadowSmoothWidthValue,
	const FDepthFieldGlowInfo& GlowInfo,
	ESimpleElementBlendMode BlendMode
	)
{
	FSimpleElementMaskedGammaBasePS::SetParameters(RHICmdList, Texture,Gamma,ClipRef,BlendMode);
	SetShaderValue(RHICmdList, GetPixelShader(),SmoothWidth,SmoothWidthValue);		
	SetPixelShaderBool(RHICmdList, GetPixelShader(),EnableShadow,EnableShadowValue);
	if (EnableShadowValue)
	{
		SetShaderValue(RHICmdList, GetPixelShader(),ShadowDirection,ShadowDirectionValue);
		SetShaderValue(RHICmdList, GetPixelShader(),ShadowColor,ShadowColorValue);
		SetShaderValue(RHICmdList, GetPixelShader(),ShadowSmoothWidth,ShadowSmoothWidthValue);
	}
	SetPixelShaderBool(RHICmdList, GetPixelShader(),EnableGlow,GlowInfo.bEnableGlow);
	if (GlowInfo.bEnableGlow)
	{
		SetShaderValue(RHICmdList, GetPixelShader(),GlowColor,GlowInfo.GlowColor);
		SetShaderValue(RHICmdList, GetPixelShader(),GlowOuterRadius,GlowInfo.GlowOuterRadius);
		SetShaderValue(RHICmdList, GetPixelShader(),GlowInnerRadius,GlowInfo.GlowInnerRadius);
	}

	// This shader does not use editor compositing
	SetEditorCompositingParameters(RHICmdList, NULL, FTexture2DRHIRef() );
}

/**
* Serialize constant paramaters for this shader
* 
* @param Ar - archive to serialize to
* @return true if any of the parameters were outdated
*/
bool FSimpleElementDistanceFieldGammaPS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FSimpleElementMaskedGammaBasePS::Serialize(Ar);
	Ar << SmoothWidth;
	Ar << EnableShadow;
	Ar << ShadowDirection;
	Ar << ShadowColor;	
	Ar << ShadowSmoothWidth;
	Ar << EnableGlow;
	Ar << GlowColor;
	Ar << GlowOuterRadius;
	Ar << GlowInnerRadius;
	return bShaderHasOutdatedParameters;
}
	
FSimpleElementHitProxyPS::FSimpleElementHitProxyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	FGlobalShader(Initializer)
{
	InTexture.Bind(Initializer.ParameterMap,TEXT("InTexture"), SPF_Mandatory);
	InTextureSampler.Bind(Initializer.ParameterMap,TEXT("InTextureSampler"));
}

void FSimpleElementHitProxyPS::SetParameters(FRHICommandList& RHICmdList, const FTexture* TextureValue)
{
	SetTextureParameter(RHICmdList, GetPixelShader(),InTexture,InTextureSampler,TextureValue);
}

bool FSimpleElementHitProxyPS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
	Ar << InTexture;
	Ar << InTextureSampler;
	return bShaderHasOutdatedParameters;
}


FSimpleElementColorChannelMaskPS::FSimpleElementColorChannelMaskPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
: FGlobalShader(Initializer)
{
	InTexture.Bind(Initializer.ParameterMap,TEXT("InTexture"), SPF_Mandatory);
	InTextureSampler.Bind(Initializer.ParameterMap,TEXT("InTextureSampler"));
	ColorWeights.Bind(Initializer.ParameterMap,TEXT("ColorWeights"));
	Gamma.Bind(Initializer.ParameterMap,TEXT("Gamma"));
}

/**
* Sets all the constant parameters for this shader
*
* @param Texture - 2d tile texture
* @param ColorWeights - reference value to compare with alpha for killing pixels
* @param Gamma - if gamma != 1.0 then a pow(color,Gamma) is applied
*/
void FSimpleElementColorChannelMaskPS::SetParameters(FRHICommandList& RHICmdList, const FTexture* TextureValue, const FMatrix& ColorWeightsValue, float GammaValue)
{
	SetTextureParameter(RHICmdList, GetPixelShader(),InTexture,InTextureSampler,TextureValue);
	SetShaderValue(RHICmdList, GetPixelShader(),ColorWeights,ColorWeightsValue);
	SetShaderValue(RHICmdList, GetPixelShader(),Gamma,GammaValue);
}

bool FSimpleElementColorChannelMaskPS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
	Ar << InTexture;
	Ar << InTextureSampler;
	Ar << ColorWeights;
	Ar << Gamma;
	return bShaderHasOutdatedParameters;
}

/*------------------------------------------------------------------------------
	Shader implementations.
------------------------------------------------------------------------------*/

IMPLEMENT_SHADER_TYPE(,FSimpleElementVS,TEXT("SimpleElementVertexShader"),TEXT("Main"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(,FSimpleElementPS, TEXT("SimpleElementPixelShader"), TEXT("Main"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FSimpleElementAlphaOnlyPS, TEXT("SimpleElementPixelShader"), TEXT("AlphaOnlyMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FSimpleElementGammaPS_SRGB, TEXT("SimpleElementPixelShader"), TEXT("GammaMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FSimpleElementGammaPS_Linear, TEXT("SimpleElementPixelShader"), TEXT("GammaMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FSimpleElementGammaAlphaOnlyPS, TEXT("SimpleElementPixelShader"), TEXT("GammaAlphaOnlyMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FSimpleElementMaskedGammaPS_SRGB, TEXT("SimpleElementPixelShader"), TEXT("GammaMaskedMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FSimpleElementMaskedGammaPS_Linear, TEXT("SimpleElementPixelShader"), TEXT("GammaMaskedMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FSimpleElementDistanceFieldGammaPS,TEXT("SimpleElementPixelShader"),TEXT("GammaDistanceFieldMain"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FSimpleElementHitProxyPS,TEXT("SimpleElementHitProxyPixelShader"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FSimpleElementColorChannelMaskPS,TEXT("SimpleElementColorChannelMaskPixelShader"),TEXT("Main"),SF_Pixel);

