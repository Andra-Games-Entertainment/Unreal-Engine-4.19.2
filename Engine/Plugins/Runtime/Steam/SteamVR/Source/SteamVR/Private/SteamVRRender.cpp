// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
//
#include "CoreMinimal.h"
#include "SteamVRPrivate.h"
#include "SteamVRHMD.h"

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "PostProcess/PostProcessHMD.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"

#if PLATFORM_LINUX
#include "VulkanRHIPrivate.h"
#include "ScreenRendering.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#endif

#if PLATFORM_MAC
#include <Metal/Metal.h>
#endif

#if STEAMVR_SUPPORTED_PLATFORMS

#if !PLATFORM_MAC
const int32 PostPresentHandoffDefault = 0;
#else
const int32 PostPresentHandoffDefault = 1;
#endif

static TAutoConsoleVariable<int32> CUsePostPresentHandoff(TEXT("vr.SteamVR.UsePostPresentHandoff"), PostPresentHandoffDefault, TEXT("Whether or not to use PostPresentHandoff.  If true, more GPU time will be available, but this relies on no SceneCaptureComponent2D or WidgetComponents being active in the scene.  Otherwise, it will break async reprojection."));

void FSteamVRHMD::DrawDistortionMesh_RenderThread(struct FRenderingCompositePassContext& Context, const FIntPoint& TextureSize)
{
	check(0);
}

void FSteamVRHMD::RenderTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef BackBuffer, FTexture2DRHIParamRef SrcTexture) const
{
	check(IsInRenderingThread());
	const_cast<FSteamVRHMD*>(this)->UpdateLayerTextures();

	if (bSplashIsShown)
	{
		SetRenderTarget(RHICmdList, SrcTexture, FTextureRHIRef());
		DrawClearQuad(RHICmdList, GMaxRHIFeatureLevel, FLinearColor(0, 0, 0, 0));
	}

	static const auto CLeftEyeOverscan = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.SteamVR.LeftEyeOverscan"));
	const bool bIsOverscan = CLeftEyeOverscan && CLeftEyeOverscan->GetValueOnAnyThread() == 1;

	static const auto CVarMirrorMode = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.MirrorMode"));
	const int32 MaxMode = (bIsOverscan) ? 3 : 2;
	const int32 WindowMirrorMode = FMath::Clamp(CVarMirrorMode->GetValueOnRenderThread(), 0, MaxMode);

	if (WindowMirrorMode != 0)
	{
		const uint32 ViewportWidth = BackBuffer->GetSizeX();
		const uint32 ViewportHeight = BackBuffer->GetSizeY();

		SetRenderTarget(RHICmdList, BackBuffer, FTextureRHIRef());
		RHICmdList.SetViewport(0, 0, 0, ViewportWidth, ViewportHeight, 1.0f);

		if (WindowMirrorMode == 1)
		{
			// need to clear when rendering only one eye since the borders won't be touched by the DrawRect below
			DrawClearQuad(RHICmdList, GMaxRHIFeatureLevel, FLinearColor::Black);
		}

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		const auto FeatureLevel = GMaxRHIFeatureLevel;
		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
		TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SrcTexture);

		if (WindowMirrorMode == 1)
		{
			FFrameSettings FS;
			{
				FScopeLock Lock(&FrameSettingsLock);
				FS = pBridge->GetFrameSettings(1);
			}

			// Take the middle 60% of the frame, so that we don't see the hidden area meshes
			float uSize = (float)FS.EyeViewports[0].Max.X / (float)FS.RenderTargetSize.X;
 			const float uMin = 0.2f * uSize;
 			uSize *= 0.6f;
 			
 			float vSize = (float)FS.EyeViewports[0].Max.Y / (float)FS.RenderTargetSize.Y;
 			const float vMin = 0.2f * vSize;
 			vSize *= 0.6f;

			RendererModule->DrawRectangle(
				RHICmdList,
				ViewportWidth / 4, 0,
				ViewportWidth / 2, ViewportHeight,
				uMin, vMin,
				uSize, vSize,
				FIntPoint(ViewportWidth, ViewportHeight),
				FIntPoint(1, 1),
				*VertexShader,
				EDRF_Default);
		}
		else if (WindowMirrorMode == 2)
		{
			RendererModule->DrawRectangle(
				RHICmdList,
				0, 0,
				ViewportWidth, ViewportHeight,
				0.0f, 0.0f,
				1.0f, 1.0f,
				FIntPoint(ViewportWidth, ViewportHeight),
				FIntPoint(1, 1),
				*VertexShader,
				EDRF_Default);
		}

		// For overscan
		else if (WindowMirrorMode == 3)
		{
			float OverscanWidth = 0.5f;
			if (bIsOverscan)
			{
				FScopeLock FrameLock(&OverscanMutex);
				OverscanWidth = static_cast<float>(RenderOverscanState.LeftEyeWidth) / static_cast<float>(RenderOverscanState.FamilyWidth);
			}

			RendererModule->DrawRectangle(
				RHICmdList,
				0, 0,
				ViewportWidth, ViewportHeight,
				0.0f, 0.0f,
				OverscanWidth, 1.0f,
				FIntPoint(ViewportWidth, ViewportHeight),
				FIntPoint(1, 1),
				*VertexShader,
				EDRF_Default);
		}
	}
}

static void DrawOcclusionMesh(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass, const FHMDViewMesh MeshAssets[])
{
	check(IsInRenderingThread());
	check(StereoPass != eSSP_FULL);

	const uint32 MeshIndex = (StereoPass == eSSP_LEFT_EYE) ? 0 : 1;
	const FHMDViewMesh& Mesh = MeshAssets[MeshIndex];
	check(Mesh.IsValid());

	DrawIndexedPrimitiveUP(
		RHICmdList,
		PT_TriangleList,
		0,
		Mesh.NumVertices,
		Mesh.NumTriangles,
		Mesh.pIndices,
		sizeof(Mesh.pIndices[0]),
		Mesh.pVertices,
		sizeof(Mesh.pVertices[0])
		);
}

void FSteamVRHMD::DrawHiddenAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
{
	DrawOcclusionMesh(RHICmdList, StereoPass, HiddenAreaMeshes);
}

void FSteamVRHMD::DrawVisibleAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
{
	DrawOcclusionMesh(RHICmdList, StereoPass, VisibleAreaMeshes);
}

void FSteamVRHMD::BridgeBaseImpl::UpdateFrameSettings(FSteamVRHMD::FFrameSettings& NewSettings)
{
	FrameSettingsStack.Add(NewSettings);
	if (FrameSettingsStack.Num() > 3)
	{
		FrameSettingsStack.RemoveAt(0);
	}
}

FSteamVRHMD::FFrameSettings FSteamVRHMD::BridgeBaseImpl::GetFrameSettings(int32 NumBufferedFrames/*=0*/)
{
	check(FrameSettingsStack.Num() > 0);
	if (NumBufferedFrames < FrameSettingsStack.Num())
	{
		return FrameSettingsStack[NumBufferedFrames];
	}
	else
	{
		// Until we build a buffer of adequate size, stick with the last submitted
		return FrameSettingsStack[0];
	}
}

#if PLATFORM_WINDOWS

FSteamVRHMD::D3D11Bridge::D3D11Bridge(FSteamVRHMD* plugin):
	BridgeBaseImpl(plugin),
	RenderTargetTexture(nullptr)
{
}

void FSteamVRHMD::D3D11Bridge::BeginRendering()
{
	check(IsInRenderingThread());

	static bool Inited = false;
	if (!Inited)
	{
		Inited = true;
	}
}

void FSteamVRHMD::D3D11Bridge::FinishRendering()
{
	static const auto CLeftEyeOverscan = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.SteamVR.LeftEyeOverscan"));
	const bool bIsOverscan = CLeftEyeOverscan && CLeftEyeOverscan->GetValueOnAnyThread() == 1;

	float OverscanWidth = 0.0f;
	float OverscanHeight = 0.0f;
	float RightEyeScale = 0.0f;
	if (bIsOverscan)
	{
		FScopeLock FrameLock(&Plugin->OverscanMutex);
		OverscanWidth = static_cast<float>(Plugin->RenderOverscanState.LeftEyeWidth) / static_cast<float>(Plugin->RenderOverscanState.FamilyWidth);
		OverscanHeight = 1.0f / Plugin->RenderOverscanState.ModifiedEyeAspectRatio;
		RightEyeScale = Plugin->RenderOverscanState.RightEyeScale;
	}

	FFrameSettings FS = GetFrameSettings(1);

	vr::Texture_t Texture;
	Texture.handle = RenderTargetTexture;
	Texture.eType = vr::TextureType_DirectX;
	Texture.eColorSpace = vr::ColorSpace_Auto;

	vr::VRTextureBounds_t LeftBounds;
	if(!bIsOverscan)
	{
		LeftBounds.uMin = 0.0f;
		LeftBounds.uMax = (float)FS.EyeViewports[0].Max.X / (float)FS.RenderTargetSize.X;
		LeftBounds.vMin = 0.0f;
		LeftBounds.vMax = (float)FS.EyeViewports[0].Max.Y / (float)FS.RenderTargetSize.Y;
	}
	else
	{
		LeftBounds.uMin = 0.0f;
		LeftBounds.uMax = OverscanWidth;
		LeftBounds.vMin = 0.0f - OverscanHeight;
		LeftBounds.vMax = 1.0f + OverscanHeight;
	}
	vr::EVRCompositorError Error = Plugin->VRCompositor->Submit(vr::Eye_Left, &Texture, &LeftBounds);

	vr::VRTextureBounds_t RightBounds;
	if (!bIsOverscan)
	{
		RightBounds.uMin = (float)FS.EyeViewports[1].Min.X / (float)FS.RenderTargetSize.X;;
		RightBounds.uMax = 1.0f;
		RightBounds.vMin = 0.0f;
		RightBounds.vMax = (float)FS.EyeViewports[1].Max.Y / (float)FS.RenderTargetSize.Y;
	}
	else
	{
		RightBounds.uMin = OverscanWidth;
		RightBounds.uMax = FMath::Lerp(OverscanWidth, 1.0f, RightEyeScale);
		RightBounds.vMin = 0.0f;
		RightBounds.vMax = 1.0f * RightEyeScale;
	}
	Texture.handle = RenderTargetTexture;
	Error = Plugin->VRCompositor->Submit(vr::Eye_Right, &Texture, &RightBounds);
	
	if (Error != vr::VRCompositorError_None)
	{
		UE_LOG(LogHMD, Log, TEXT("Warning:  SteamVR Compositor had an error on present (%d)"), (int32)Error);
	}
}

void FSteamVRHMD::D3D11Bridge::Reset()
{
}

void FSteamVRHMD::D3D11Bridge::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());
	check(InViewportRHI);

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RT));

	if (RenderTargetTexture != nullptr)
	{
		RenderTargetTexture->Release();	//@todo steamvr: need to also release in reset
	}

	RenderTargetTexture = (ID3D11Texture2D*)RT->GetNativeResource();
	RenderTargetTexture->AddRef();

	InViewportRHI->SetCustomPresent(this);
}


void FSteamVRHMD::D3D11Bridge::OnBackBufferResize()
{
}

bool FSteamVRHMD::D3D11Bridge::Present(int& SyncInterval)
{
	check(IsInRenderingThread());

	if (Plugin->VRCompositor == nullptr)
	{
		return false;
	}

	FinishRendering();

	return true;
}

void FSteamVRHMD::D3D11Bridge::PostPresent()
{
	if (CUsePostPresentHandoff.GetValueOnRenderThread() == 1)
	{
		Plugin->VRCompositor->PostPresentHandoff();
	}
}

#elif PLATFORM_LINUX
#if STEAMVR_USE_VULKAN_RHI
FSteamVRHMD::VulkanBridge::VulkanBridge(FSteamVRHMD* plugin):
	BridgeBaseImpl(plugin),
	RenderTargetTexture(0)
{
	bInitialized = true;
}

void FSteamVRHMD::VulkanBridge::BeginRendering()
{
//	check(IsInRenderingThread());
}

void FSteamVRHMD::VulkanBridge::FinishRendering()
{
	auto vlkRHI = static_cast<FVulkanDynamicRHI*>(GDynamicRHI);

	if(RenderTargetTexture.IsValid())
	{
		FVulkanTexture2D* Texture2D = (FVulkanTexture2D*)RenderTargetTexture.GetReference();
		FVulkanCommandListContext& ImmediateContext = vlkRHI->GetDevice()->GetImmediateContext();
		const VkImageLayout* CurrentLayout = ImmediateContext.GetTransitionState().CurrentLayout.Find(Texture2D->Surface.Image);
		FVulkanCmdBuffer* CmdBuffer = ImmediateContext.GetCommandBufferManager()->GetUploadCmdBuffer();
		VulkanSetImageLayoutSimple(CmdBuffer->GetHandle(), Texture2D->Surface.Image, CurrentLayout ? *CurrentLayout : VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		FFrameSettings FS = GetFrameSettings();

		vr::VRTextureBounds_t LeftBounds;
		LeftBounds.uMin = 0.0f;
		LeftBounds.uMax = (float)FS.EyeViewports[0].Max.X / (float)FS.RenderTargetSize.X;
		LeftBounds.vMin = 0.0f;
		LeftBounds.vMax = (float)FS.EyeViewports[0].Max.Y / (float)FS.RenderTargetSize.Y;


		vr::VRTextureBounds_t RightBounds;
		RightBounds.uMin = (float)FS.EyeViewports[1].Min.X / (float)FS.RenderTargetSize.X;;
		RightBounds.uMax = 1.0f;
		RightBounds.vMin = 0.0f;
		RightBounds.vMax = (float)FS.EyeViewports[1].Max.Y / (float)FS.RenderTargetSize.Y;

		vr::VRVulkanTextureData_t vulkanData {};
		vulkanData.m_pInstance			= vlkRHI->GetInstance();
		vulkanData.m_pDevice			= vlkRHI->GetDevice()->GetInstanceHandle();
		vulkanData.m_pPhysicalDevice	= vlkRHI->GetDevice()->GetPhysicalHandle();
		vulkanData.m_pQueue				= vlkRHI->GetDevice()->GetGraphicsQueue()->GetHandle();
		vulkanData.m_nQueueFamilyIndex	= vlkRHI->GetDevice()->GetGraphicsQueue()->GetFamilyIndex();
		vulkanData.m_nImage				= (uint64_t)Texture2D->Surface.Image;
		vulkanData.m_nWidth				= Texture2D->Surface.Width;
		vulkanData.m_nHeight			= Texture2D->Surface.Height;
		vulkanData.m_nFormat			= (uint32_t)Texture2D->Surface.ViewFormat;
		vulkanData.m_nSampleCount = 1;

		vr::Texture_t texture = {&vulkanData, vr::TextureType_Vulkan, vr::ColorSpace_Auto};

		Plugin->VRCompositor->Submit(vr::Eye_Left, &texture, &LeftBounds);
		Plugin->VRCompositor->Submit(vr::Eye_Right, &texture, &RightBounds);

	}
}

void FSteamVRHMD::VulkanBridge::Reset()
{

}

void FSteamVRHMD::VulkanBridge::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	RenderTargetTexture = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RenderTargetTexture));

	InViewportRHI->SetCustomPresent(this);
}


void FSteamVRHMD::VulkanBridge::OnBackBufferResize()
{
}

bool FSteamVRHMD::VulkanBridge::Present(int& SyncInterval)
{
	if (Plugin->VRCompositor == nullptr)
	{
		return false;
	}

	FinishRendering();

	return true;
}

void FSteamVRHMD::VulkanBridge::PostPresent()
{
	if (CUsePostPresentHandoff.GetValueOnRenderThread() == 1)
	{
		Plugin->VRCompositor->PostPresentHandoff();
	}
}
#else
FSteamVRHMD::OpenGLBridge::OpenGLBridge(FSteamVRHMD* plugin):
	BridgeBaseImpl(plugin),
	RenderTargetTexture(0)
{
	bInitialized = true;
}

void FSteamVRHMD::OpenGLBridge::BeginRendering()
{
	check(IsInRenderingThread());


}

void FSteamVRHMD::OpenGLBridge::FinishRendering()
{
	// Yaakuro:
	// TODO This is a workaround. After exiting VR Editor the texture gets invalid at some point.
	// Need to find it. This at least prevents to use this method when texture name is not valid anymore.
	if(!glIsTexture(RenderTargetTexture))
	{
		return;
	}

	FFrameSettings FS = GetFrameSettings();

	vr::VRTextureBounds_t LeftBounds;
	LeftBounds.uMin = 0.0f;
	LeftBounds.uMax = (float)FS.EyeViewports[0].Max.X / (float)FS.RenderTargetSize.X;
	LeftBounds.vMin = (float)FS.EyeViewports[0].Max.Y / (float)FS.RenderTargetSize.Y;
	LeftBounds.vMax = 0.0f;

	vr::VRTextureBounds_t RightBounds;
	RightBounds.uMin = (float)FS.EyeViewports[1].Min.X / (float)FS.RenderTargetSize.X;;
	RightBounds.uMax = 1.0f;
	RightBounds.vMin = (float)FS.EyeViewports[1].Max.Y / (float)FS.RenderTargetSize.Y;
	RightBounds.vMax = 0.0f;

	vr::Texture_t Texture;
	Texture.handle = reinterpret_cast<void*>(RenderTargetTexture);
	Texture.eType = vr::TextureType_OpenGL;
	Texture.eColorSpace = vr::ColorSpace_Auto;

	Plugin->VRCompositor->Submit(vr::Eye_Left, &Texture, &LeftBounds);
	Plugin->VRCompositor->Submit(vr::Eye_Right, &Texture, &RightBounds);
}

void FSteamVRHMD::OpenGLBridge::Reset()
{
	RenderTargetTexture = 0;
}

void FSteamVRHMD::OpenGLBridge::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RT));

	RenderTargetTexture = *reinterpret_cast<GLuint*>(RT->GetNativeResource());

	InViewportRHI->SetCustomPresent(this);
}


void FSteamVRHMD::OpenGLBridge::OnBackBufferResize()
{
}

bool FSteamVRHMD::OpenGLBridge::Present(int& SyncInterval)
{
	check(IsInRenderingThread());

	if (Plugin->VRCompositor == nullptr)
	{
		return false;
	}

	FinishRendering();

	return true;
}

void FSteamVRHMD::OpenGLBridge::PostPresent()
{
	if (CUsePostPresentHandoff.GetValueOnRenderThread() == 1)
	{
		Plugin->VRCompositor->PostPresentHandoff();
	}
}
#endif

#endif // PLATFORM_WINDOWS

#if PLATFORM_MAC

FSteamVRHMD::MetalBridge::MetalBridge(FSteamVRHMD* plugin):
	BridgeBaseImpl(plugin)
{}

void FSteamVRHMD::MetalBridge::BeginRendering()
{
	check(IsInRenderingThread());

	static bool Inited = false;
	if (!Inited)
	{
		Inited = true;
	}
}

void FSteamVRHMD::MetalBridge::FinishRendering()
{
	if(IsOnLastPresentedFrame())
	{
		return;
	}
	
	LastPresentedFrameNumber = GetFrameNumber();
	
	check(TextureSet.IsValid());
	
	static const auto CLeftEyeOverscan = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.SteamVR.LeftEyeOverscan"));
	const bool bIsOverscan = CLeftEyeOverscan && CLeftEyeOverscan->GetValueOnAnyThread() == 1;
	float OverscanWidth = 0.0f;
	float OverscanHeight = 0.0f;
	float RightEyeScale = 0.0f;
	if (bIsOverscan)
	{
		FScopeLock FrameLock(&Plugin->OverscanMutex);
		OverscanWidth = static_cast<float>(Plugin->RenderOverscanState.LeftEyeWidth) / static_cast<float>(Plugin->RenderOverscanState.FamilyWidth);
		OverscanHeight = 1.0f / Plugin->RenderOverscanState.ModifiedEyeAspectRatio;
		RightEyeScale = Plugin->RenderOverscanState.RightEyeScale;
	}

	FFrameSettings FS = GetFrameSettings();

	vr::VRTextureBounds_t LeftBounds;
	if(!bIsOverscan)
	{
		LeftBounds.uMin = 0.0f;
		LeftBounds.uMax = (float)FS.EyeViewports[0].Max.X / (float)FS.RenderTargetSize.X;
		LeftBounds.vMin = 0.0f;
		LeftBounds.vMax = (float)FS.EyeViewports[0].Max.Y / (float)FS.RenderTargetSize.Y;
	}
	else
	{
		LeftBounds.uMin = 0.0f;
		LeftBounds.uMax = OverscanWidth;
		LeftBounds.vMin = 0.0f - OverscanHeight;
		LeftBounds.vMax = 1.0f + OverscanHeight;
	}
	
	id<MTLTexture> TextureHandle = (id<MTLTexture>)TextureSet->GetNativeResource();
	
	vr::Texture_t Texture;
	Texture.handle = (void*)TextureHandle.iosurface;
	Texture.eType = vr::TextureType_IOSurface;
	Texture.eColorSpace = vr::ColorSpace_Auto;
	vr::EVRCompositorError Error = Plugin->VRCompositor->Submit(vr::Eye_Left, &Texture, &LeftBounds);

	vr::VRTextureBounds_t RightBounds;
	if (!bIsOverscan)
	{
		RightBounds.uMin = (float)FS.EyeViewports[1].Min.X / (float)FS.RenderTargetSize.X;
		RightBounds.uMax = 1.0f;
		RightBounds.vMin = 0.0f;
		RightBounds.vMax = (float)FS.EyeViewports[1].Max.Y / (float)FS.RenderTargetSize.Y;
	}
	else
	{
		RightBounds.uMin = OverscanWidth;
		RightBounds.uMax = FMath::Lerp(OverscanWidth, 1.0f, RightEyeScale);
		RightBounds.vMin = 0.0f;
		RightBounds.vMax = 1.0f * RightEyeScale;
	}

	Error = Plugin->VRCompositor->Submit(vr::Eye_Right, &Texture, &RightBounds);
	if (Error != vr::VRCompositorError_None)
	{
		UE_LOG(LogHMD, Log, TEXT("Warning:  SteamVR Compositor had an error on present (%d)"), (int32)Error);
	}

	static_cast<FRHITextureSet2D*>(TextureSet.GetReference())->Advance();
}

void FSteamVRHMD::MetalBridge::Reset()
{
}

void FSteamVRHMD::MetalBridge::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());
	check(InViewportRHI);
	InViewportRHI->SetCustomPresent(this);
}

void FSteamVRHMD::MetalBridge::OnBackBufferResize()
{
}

bool FSteamVRHMD::MetalBridge::Present(int& SyncInterval)
{
	// Editor appears to be in rt, game appears to be in rhi?
	//check(IsInRenderingThread());
	//check(IsInRHIThread());

	if (Plugin->VRCompositor == nullptr)
	{
		return false;
	}

	FinishRendering();

	return true;
}

void FSteamVRHMD::MetalBridge::PostPresent()
{
	if (CUsePostPresentHandoff.GetValueOnRenderThread() == 1)
	{
		Plugin->VRCompositor->PostPresentHandoff();
	}
}

IOSurfaceRef FSteamVRHMD::MetalBridge::GetSurface(const uint32 SizeX, const uint32 SizeY)
{
	const NSDictionary* SurfaceDefinition = @{
											(id)kIOSurfaceWidth: @(SizeX),
											(id)kIOSurfaceHeight: @(SizeY),
											(id)kIOSurfaceBytesPerElement: @(4), // sizeof(PF_B8G8R8A8)..
											(id)kIOSurfaceIsGlobal: @YES
											};
	
	return IOSurfaceCreate((CFDictionaryRef)SurfaceDefinition);
}

#endif // PLATFORM_MAC

#endif // STEAMVR_SUPPORTED_PLATFORMS
