// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TranslucentRendering.h: Translucent rendering definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HitProxies.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "SceneRendering.h"
#include "VolumeRendering.h"

/**
* Translucent draw policy factory.
* Creates the policies needed for rendering a mesh based on its material
*/
class FTranslucencyDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = true };
	struct ContextType 
	{
		const FProjectedShadowInfo* TranslucentSelfShadow;
		ETranslucencyPass::Type TranslucenyPassType;
		ESceneRenderTargetsMode::Type TextureMode;
		bool bSceneColorCopyIsUpToDate;
		bool bPostAA;

		ContextType(const FProjectedShadowInfo* InTranslucentSelfShadow = NULL, ETranslucencyPass::Type InTranslucenyPassType = ETranslucencyPass::TPT_StandardTranslucency, bool bPostAAIn = false, ESceneRenderTargetsMode::Type InTextureMode = ESceneRenderTargetsMode::SetTextures)
			: TranslucentSelfShadow(InTranslucentSelfShadow)
			, TranslucenyPassType(InTranslucenyPassType)
			, TextureMode(InTextureMode)
			, bSceneColorCopyIsUpToDate(false)
			, bPostAA(bPostAAIn)			
		{}
	};

	/**
	* Render a dynamic mesh using a translucent draw policy 
	* @return true if the mesh rendered
	*/
	static bool DrawDynamicMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		bool bPreFog,
		const FDrawingPolicyRenderState& DrawRenderState,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);

	/**
	* Render a dynamic mesh using a translucent draw policy 
	* @return true if the mesh rendered
	*/
	static bool DrawStaticMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		ContextType DrawingContext,
		const FStaticMesh& StaticMesh,
		const uint64& BatchElementMask,
		bool bPreFog,
		const FDrawingPolicyRenderState& DrawRenderState,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);

	/**
	* Resolves the scene color target and copies it for use as a source texture.
	*/
	static void CopySceneColor(FRHICommandList& RHICmdList, const FViewInfo& View, const FPrimitiveSceneProxy* PrimitiveSceneProxy);

private:
	/**
	* Render a dynamic or static mesh using a translucent draw policy
	* @return true if the mesh rendered
	*/
	static bool DrawMesh(
		FRHICommandList& RHICmdList,
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		const uint64& BatchElementMask,
		const FDrawingPolicyRenderState& DrawRenderState,
		bool bPreFog,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);

};


/**
* Translucent draw policy factory.
* Creates the policies needed for rendering a mesh based on its material
*/
class FMobileTranslucencyDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = true };
	struct ContextType 
	{
		ESceneRenderTargetsMode::Type TextureMode;
		bool bRenderingSeparateTranslucency;

		ContextType(ESceneRenderTargetsMode::Type InTextureMode, bool InbRenderingSeparateTranslucency)
		: TextureMode(InTextureMode)
		, bRenderingSeparateTranslucency(InbRenderingSeparateTranslucency)
		{}

		bool ShouldRenderSeparateTranslucency() const { return bRenderingSeparateTranslucency; }
	};

	/**
	* Render a dynamic mesh using a translucent draw policy 
	* @return true if the mesh rendered
	*/
	static bool DrawDynamicMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		bool bPreFog,
		const FDrawingPolicyRenderState& DrawRenderState,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);
};