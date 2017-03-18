// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SlateRHIRenderer.h"
#include "Fonts/FontCache.h"
#include "SlateRHIRenderingPolicy.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "EngineGlobals.h"
#include "RendererInterface.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "RHIStaticStates.h"
#include "UnrealEngine.h"
#include "GlobalShader.h"
#include "ScreenRendering.h"
#include "SlateShaders.h"
#include "Rendering/ElementBatcher.h"
#include "StereoRendering.h"
#include "Features/ILiveStreamingService.h"
#include "SlateNativeTextureResource.h"
#include "SceneUtils.h"
#include "Runtime/Renderer/Public/VolumeRendering.h"
#include "ShaderCompiler.h"

DECLARE_CYCLE_STAT(TEXT("Map Staging Buffer"),STAT_MapStagingBuffer,STATGROUP_CrashTracker);
DECLARE_CYCLE_STAT(TEXT("Generate Capture Buffer"),STAT_GenerateCaptureBuffer,STATGROUP_CrashTracker);
DECLARE_CYCLE_STAT(TEXT("Unmap Staging Buffer"),STAT_UnmapStagingBuffer,STATGROUP_CrashTracker);

DECLARE_CYCLE_STAT(TEXT("Slate RT: Rendering"), STAT_SlateRenderingRTTime, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("Slate RT: Create Batches"), STAT_SlateRTCreateBatches, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("Slate RT: Fill Vertex & Index Buffers"), STAT_SlateRTFillVertexIndexBuffers, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("Slate RT: Draw Batches"), STAT_SlateRTDrawBatches, STATGROUP_Slate);

DECLARE_FLOAT_COUNTER_STAT(TEXT("Slate UI"), Stat_GPU_SlateUI, STATGROUP_GPU); 

// Defines the maximum size that a slate viewport will create
#define MAX_VIEWPORT_SIZE 16384

static TAutoConsoleVariable<float> CVarUILevel(
	TEXT("r.HDR.UI.Level"),
	1.0f,
	TEXT("Luminance level for UI elements when compositing into HDR framebuffer (default: 1.0)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarUICompositeMode(
	TEXT("r.HDR.UI.CompositeMode"),
	1,
	TEXT("Mode used when compositing the UI layer:\n")
	TEXT("0: Standard compositing\n")
	TEXT("1: Shader pass to improve HDR blending\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarDrawToVRRenderTarget(
	TEXT("Slate.DrawToVRRenderTarget"),
	1,
	TEXT("If enabled while in VR. Slate UI will be drawn into the render target texture where the VR imagery for either eye was rendered, allow the viewer of the HMD to see the UI (for better or worse.)  This render target will then be cropped/scaled into the back buffer, if mirroring is enabled.  When disabled, Slate UI will be drawn on top of the backbuffer (not to the HMD) after the mirror texture has been cropped/scaled into the backbuffer."),
	ECVF_RenderThreadSafe);


void FSlateCrashReportResource::InitDynamicRHI()
{
	int32 Width = VirtualScreen.Width();
	int32 Height = VirtualScreen.Height();

	FRHIResourceCreateInfo CreateInfo;
	CrashReportBuffer = RHICreateTexture2D(
		Width,
		Height,
		PF_R8G8B8A8,
		1,
		1,
		TexCreate_RenderTargetable,
		CreateInfo
		);

	for (int32 i = 0; i < 2; ++i)
	{
		ReadbackBuffer[i] = RHICreateTexture2D(
			Width,
			Height,
			PF_R8G8B8A8,
			1,
			1,
			TexCreate_CPUReadback,
			CreateInfo
			);
	}
	
	ReadbackBufferIndex = 0;
}

void FSlateCrashReportResource::ReleaseDynamicRHI()
{
	ReadbackBuffer[0].SafeRelease();
	ReadbackBuffer[1].SafeRelease();
	CrashReportBuffer.SafeRelease();
}

FSlateWindowElementList* FSlateCrashReportResource::GetNextElementList()
{
	ElementListIndex = (ElementListIndex + 1) % 2;
	return &ElementList[ElementListIndex];
}


void FSlateRHIRenderer::FViewportInfo::InitRHI()
{
	// Viewport RHI is created on the game thread
	// Create the depth-stencil surface if needed.
	RecreateDepthBuffer_RenderThread();
}

void FSlateRHIRenderer::FViewportInfo::ReleaseRHI()
{
	DepthStencil.SafeRelease();
	ViewportRHI.SafeRelease();	
}

void FSlateRHIRenderer::FViewportInfo::ConditionallyUpdateDepthBuffer(bool bInRequiresStencilTest)
{
	FViewportInfo* ViewportInfo = this;
	ENQUEUE_RENDER_COMMAND(UpdateDepthBufferCommand)(
		[ViewportInfo, bInRequiresStencilTest](FRHICommandListImmediate& RHICmdList)
		{
			// Allocate a stencil buffer if needed and not already allocated
			if (bInRequiresStencilTest && !ViewportInfo->bRequiresStencilTest)
			{
				ViewportInfo->bRequiresStencilTest = bInRequiresStencilTest;
				ViewportInfo->RecreateDepthBuffer_RenderThread();
			}
		}
	);
}

void FSlateRHIRenderer::FViewportInfo::RecreateDepthBuffer_RenderThread()
{
	check(IsInRenderingThread());
	DepthStencil.SafeRelease();
	if (bRequiresStencilTest)
	{		
		FTexture2DRHIRef ShaderResourceUnused;
		FRHIResourceCreateInfo CreateInfo(FClearValueBinding::DepthZero);
		RHICreateTargetableShaderResource2D( Width, Height, PF_DepthStencil, 1, TexCreate_None, TexCreate_DepthStencilTargetable, false, CreateInfo, DepthStencil, ShaderResourceUnused );
		check( IsValidRef(DepthStencil) );
	}
}



FSlateRHIRenderer::FSlateRHIRenderer( TSharedRef<FSlateFontServices> InSlateFontServices, TSharedRef<FSlateRHIResourceManager> InResourceManager )
	: FSlateRenderer(InSlateFontServices)
	, EnqueuedWindowDrawBuffer(NULL)
	, FreeBufferIndex(0)
	, CurrentSceneIndex(-1)
{
	ResourceManager = InResourceManager;

	CrashTrackerResource = NULL;
	ViewMatrix = FMatrix(	FPlane(1,	0,	0,	0),
							FPlane(0,	1,	0,	0),
							FPlane(0,	0,	1,  0),
							FPlane(0,	0,	0,	1));

	bTakingAScreenShot = false;
	OutScreenshotData = NULL;
}

FSlateRHIRenderer::~FSlateRHIRenderer()
{
}

FMatrix FSlateRHIRenderer::CreateProjectionMatrix(uint32 Width, uint32 Height)
{
	// Create ortho projection matrix
	const float Left = 0;
	const float Right = Left + Width;
	const float Top = 0;
	const float Bottom = Top + Height;
	const float ZNear = -100.0f;
	const float ZFar = 100.0f;
	return AdjustProjectionMatrixForRHI(
		FMatrix(
		FPlane(2.0f / (Right - Left), 0, 0, 0),
		FPlane(0, 2.0f / (Top - Bottom), 0, 0),
		FPlane(0, 0, 1 / (ZNear - ZFar), 0),
		FPlane((Left + Right) / (Left - Right), (Top + Bottom) / (Bottom - Top), ZNear / (ZNear - ZFar), 1)
		)
		);
}

bool FSlateRHIRenderer::Initialize()
{
	LoadUsedTextures();

	RenderingPolicy = MakeShareable( new FSlateRHIRenderingPolicy( SlateFontServices.ToSharedRef(), ResourceManager.ToSharedRef() ) );

	ElementBatcher = MakeShareable( new FSlateElementBatcher( RenderingPolicy.ToSharedRef() ) );

	CurrentSceneIndex = -1;
	ActiveScenes.Empty();
	return true;
}

void FSlateRHIRenderer::Destroy()
{
	RenderingPolicy->ReleaseResources();
	ResourceManager->ReleaseResources();
	SlateFontServices->ReleaseResources();

	for( TMap< const SWindow*, FViewportInfo*>::TIterator It(WindowToViewportInfo); It; ++It )
	{
		BeginReleaseResource( It.Value() );
	}
	
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	if (CrashTrackerResource != nullptr)
	{
		BeginReleaseResource(CrashTrackerResource);
	}
#endif

	FlushRenderingCommands();
	
	check( ElementBatcher.IsUnique() );
	ElementBatcher.Reset();
	RenderingPolicy.Reset();
	ResourceManager.Reset();
	SlateFontServices.Reset();

	for( TMap< const SWindow*, FViewportInfo*>::TIterator It(WindowToViewportInfo); It; ++It )
	{
		FViewportInfo* ViewportInfo = It.Value();
		delete ViewportInfo;
	}
	
	if (CrashTrackerResource != nullptr)
	{
		delete CrashTrackerResource;
		CrashTrackerResource = nullptr;
	}

	WindowToViewportInfo.Empty();
	CurrentSceneIndex = -1;
	ActiveScenes.Empty();
}

/** Returns a draw buffer that can be used by Slate windows to draw window elements */
FSlateDrawBuffer& FSlateRHIRenderer::GetDrawBuffer()
{
	FreeBufferIndex = (FreeBufferIndex + 1) % NumDrawBuffers;
	
	FSlateDrawBuffer* Buffer = &DrawBuffers[FreeBufferIndex];
	
	while( !Buffer->Lock() )
	{
		// If the buffer cannot be locked then the buffer is still in use.  If we are here all buffers are in use
		// so wait until one is free.
		if (IsInSlateThread())
		{
			// We can't flush commands on the slate thread, so simply spinlock until we're done
			// this happens if the render thread becomes completely blocked by expensive tasks when the Slate thread is running
			// in this case we cannot tick Slate.
			FPlatformProcess::Sleep(0.001f);
		}
		else
		{
			FlushCommands();
			UE_LOG(LogSlate, Warning, TEXT("Slate: Had to block on waiting for a draw buffer"));
			FreeBufferIndex = (FreeBufferIndex + 1) % NumDrawBuffers;
		}
	

		Buffer = &DrawBuffers[FreeBufferIndex];
	}

	// Safely remove brushes by emptying the array and releasing references
	DynamicBrushesToRemove[FreeBufferIndex].Empty();

	Buffer->ClearBuffer();
	return *Buffer;
}

void FSlateRHIRenderer::CreateViewport( const TSharedRef<SWindow> Window )
{
	FlushRenderingCommands();

	if( !WindowToViewportInfo.Contains( &Window.Get() ) )
	{
		const FVector2D WindowSize = Window->GetViewportSize();

		// Clamp the window size to a reasonable default anything below 8 is a d3d warning and 8 is used anyway.
		// @todo Slate: This is a hack to work around menus being summoned with 0,0 for window size until they are ticked.
		const uint32 Width = FMath::Max(8,FMath::TruncToInt(WindowSize.X));
		const uint32 Height = FMath::Max(8,FMath::TruncToInt(WindowSize.Y));

		FViewportInfo* NewInfo = new FViewportInfo();
		// Create Viewport RHI if it doesn't exist (this must be done on the game thread)
		TSharedRef<FGenericWindow> NativeWindow = Window->GetNativeWindow().ToSharedRef();
		NewInfo->OSWindow = NativeWindow->GetOSWindowHandle();
		NewInfo->Width = Width;
		NewInfo->Height = Height;
		NewInfo->DesiredWidth = Width;
		NewInfo->DesiredHeight = Height;
		NewInfo->ProjectionMatrix = CreateProjectionMatrix( Width, Height );
#if ALPHA_BLENDED_WINDOWS		
		if( Window->GetTransparencySupport() == EWindowTransparency::PerPixel )
		{
			NewInfo->PixelFormat = PF_B8G8R8A8;
		}
#endif

		static const auto CVarHDROutputEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.EnableHDROutput"));

		// SDR format holds the requested format in non HDR mode
		NewInfo->SDRPixelFormat = NewInfo->PixelFormat;

		if (CVarHDROutputEnabled && CVarHDROutputEnabled->GetValueOnGameThread() != 0)
		{
			NewInfo->PixelFormat = GRHIHDRDisplayOutputFormat;
		}

		// Sanity check dimensions
		checkf(Width <= MAX_VIEWPORT_SIZE && Height <= MAX_VIEWPORT_SIZE, TEXT("Invalid window with Width=%u and Height=%u"), Width, Height);

		bool bFullscreen = IsViewportFullscreen( *Window );
		NewInfo->ViewportRHI = RHICreateViewport( NewInfo->OSWindow, Width, Height, bFullscreen, NewInfo->PixelFormat );
		NewInfo->bFullscreen = bFullscreen;

		WindowToViewportInfo.Add( &Window.Get(), NewInfo );

		BeginInitResource( NewInfo );
	}
}

void FSlateRHIRenderer::ConditionalResizeViewport( FViewportInfo* ViewInfo, uint32 Width, uint32 Height, bool bFullscreen )
{
	checkSlow( IsThreadSafeForSlateRendering() );

	// Force update if HDR output state changes
	static const auto CVarHDROutputEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.EnableHDROutput"));
	static const auto CVarHDRColorGamut = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.Display.ColorGamut"));
	static const auto CVarHDROutputDevice = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.Display.OutputDevice"));

	bool bHDREnabled = GRHISupportsHDROutput && CVarHDROutputEnabled && CVarHDROutputEnabled->GetValueOnAnyThread() != 0;
	int32 HDRColorGamut = CVarHDRColorGamut ? CVarHDRColorGamut->GetValueOnAnyThread() : 0;
	int32 HDROutputDevice = CVarHDROutputDevice ? CVarHDROutputDevice->GetValueOnAnyThread() : 0;

	bool bHDRStale = ViewInfo && (
		((ViewInfo->PixelFormat == GRHIHDRDisplayOutputFormat) != bHDREnabled)	// HDR toggled
#if PLATFORM_WINDOWS
		|| (IsRHIDeviceNVIDIA() &&												// Nvidia-specific mastering data updates
			((bHDREnabled && ViewInfo->HDRColorGamut != HDRColorGamut)			// Color gamut changed
			|| (bHDREnabled && ViewInfo->HDROutputDevice != HDROutputDevice)))	// Output device changed
#endif
		);

	if( IsInGameThread() && !IsInSlateThread() && ViewInfo && ( bHDRStale || ViewInfo->Height != Height || ViewInfo->Width != Width ||  ViewInfo->bFullscreen != bFullscreen || !IsValidRef(ViewInfo->ViewportRHI) ) )
	{
		// The viewport size we have doesn't match the requested size of the viewport.
		// Resize it now.

		// cannot resize the viewport while potentially using it.
		FlushRenderingCommands();
	
		// Windows are allowed to be zero sized ( sometimes they are animating to/from zero for example)
		// but viewports cannot be zero sized.  Use 8x8 as a reasonably sized viewport in this case.
		uint32 NewWidth = FMath::Max<uint32>( 8, Width );
		uint32 NewHeight = FMath::Max<uint32>( 8, Height );

		// Sanity check dimensions
		if (NewWidth > MAX_VIEWPORT_SIZE)
		{
			UE_LOG(LogSlate, Warning, TEXT("Tried to set viewport width size to %d.  Clamping size to max allowed size of %d instead."), NewWidth, MAX_VIEWPORT_SIZE);
			NewWidth = MAX_VIEWPORT_SIZE;
		}

		if (NewHeight > MAX_VIEWPORT_SIZE)
		{
			UE_LOG(LogSlate, Warning, TEXT("Tried to set viewport height size to %d.  Clamping size to max allowed size of %d instead."), NewHeight, MAX_VIEWPORT_SIZE);
			NewHeight = MAX_VIEWPORT_SIZE;
		}

		ViewInfo->Width = NewWidth;
		ViewInfo->Height = NewHeight;
		ViewInfo->DesiredWidth = NewWidth;
		ViewInfo->DesiredHeight = NewHeight;
		ViewInfo->ProjectionMatrix = CreateProjectionMatrix( NewWidth, NewHeight );
		ViewInfo->bFullscreen = bFullscreen;

		ViewInfo->PixelFormat = bHDREnabled ? GRHIHDRDisplayOutputFormat : ViewInfo->SDRPixelFormat;
		ViewInfo->HDRColorGamut = HDRColorGamut;
		ViewInfo->HDROutputDevice = HDROutputDevice;

		PreResizeBackBufferDelegate.Broadcast(&ViewInfo->ViewportRHI);
		if( IsValidRef( ViewInfo->ViewportRHI ) )
		{
			ensureMsgf(ViewInfo->ViewportRHI->GetRefCount() == 1, TEXT("Viewport backbuffer was not properly released"));
			RHIResizeViewport(ViewInfo->ViewportRHI, NewWidth, NewHeight, bFullscreen, ViewInfo->PixelFormat);
		}
		else
		{
			ViewInfo->ViewportRHI = RHICreateViewport(ViewInfo->OSWindow, NewWidth, NewHeight, bFullscreen, ViewInfo->PixelFormat);
		}

		PostResizeBackBufferDelegate.Broadcast(&ViewInfo->ViewportRHI);
	}
}

void FSlateRHIRenderer::UpdateFullscreenState( const TSharedRef<SWindow> Window, uint32 OverrideResX, uint32 OverrideResY )
{
	FViewportInfo* ViewInfo = WindowToViewportInfo.FindRef( &Window.Get() );

	if( !ViewInfo )
	{
		CreateViewport( Window );
	}

	ViewInfo = WindowToViewportInfo.FindRef( &Window.Get() );

	if( ViewInfo )
	{
		const bool bFullscreen = IsViewportFullscreen( *Window );
		
		uint32 ResX = OverrideResX ? OverrideResX : GSystemResolution.ResX;
		uint32 ResY = OverrideResY ? OverrideResY : GSystemResolution.ResY;

		if( (GIsEditor && Window->IsViewportSizeDrivenByWindow()) || (Window->GetWindowMode() == EWindowMode::WindowedFullscreen))
		{
			ResX = ViewInfo->Width;
			ResY = ViewInfo->Height;
		}

		ConditionalResizeViewport( ViewInfo, ResX, ResY, bFullscreen );
	}
}

void FSlateRHIRenderer::RestoreSystemResolution(const TSharedRef<SWindow> InWindow)
{
	if (!GIsEditor && InWindow->GetWindowMode() == EWindowMode::Fullscreen)
	{
		// Force the window system to resize the active viewport, even though nothing might have appeared to change.
		// On windows, DXGI might change the window resolution behind our backs when we alt-tab out. This will make
		// sure that we are actually in the resolution we think we are.
#if !PLATFORM_HTML5
		// @todo: fixme for HTML5. 
		GSystemResolution.ForceRefresh();
#endif 
	}
}

/** Called when a window is destroyed to give the renderer a chance to free resources */
void FSlateRHIRenderer::OnWindowDestroyed( const TSharedRef<SWindow>& InWindow )
{
	checkSlow(IsThreadSafeForSlateRendering());

	FViewportInfo** ViewportInfoPtr = WindowToViewportInfo.Find( &InWindow.Get() );
	if( ViewportInfoPtr )
	{
		OnSlateWindowDestroyedDelegate.Broadcast(&(*ViewportInfoPtr)->ViewportRHI);

		BeginReleaseResource(*ViewportInfoPtr);

		// Need to flush rendering commands as the viewport may be in use by the render thread
		// and the rendering resources must be released on the render thread before the viewport can be deleted
		FlushRenderingCommands();
	
		delete *ViewportInfoPtr;
	}

	WindowToViewportInfo.Remove( &InWindow.Get() );
}

// Limited platform support for HDR UI composition
bool SupportsUICompositionRendering(const EShaderPlatform Platform)
{
	return IsFeatureLevelSupported(Platform,ERHIFeatureLevel::SM5) && (RHISupportsGeometryShaders(Platform) || RHISupportsVertexShaderLayer(Platform));
}

// Pixel shader to generate LUT for HDR UI composition
class FCompositeLUTGenerationPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCompositeLUTGenerationPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return SupportsUICompositionRendering(Platform);
	}

	FCompositeLUTGenerationPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		OutputDevice.Bind(Initializer.ParameterMap, TEXT("OutputDevice"));
		OutputGamut.Bind(Initializer.ParameterMap, TEXT("OutputGamut"));
	}
	FCompositeLUTGenerationPS() {}

	void SetParameters(FRHICommandList& RHICmdList)
	{
		static const auto CVarOutputDevice = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.Display.OutputDevice"));
		static const auto CVarOutputGamut = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.Display.ColorGamut"));
		static const auto CVarOutputGamma = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.TonemapperGamma"));

		int32 OutputDeviceValue = CVarOutputDevice->GetValueOnRenderThread();
		int32 OutputGamutValue = CVarOutputGamut->GetValueOnRenderThread();
		float Gamma = CVarOutputGamma->GetValueOnRenderThread();

		if (PLATFORM_APPLE && Gamma == 0.0f)
		{
			Gamma = 2.2f;
		}
	
		if (Gamma > 0.0f)
		{
			// Enforce user-controlled ramp over sRGB or Rec709
			OutputDeviceValue = FMath::Max(OutputDeviceValue, 2);
		}

		SetShaderValue(RHICmdList, GetPixelShader(), OutputDevice, OutputDeviceValue);
		SetShaderValue(RHICmdList, GetPixelShader(), OutputGamut, OutputGamutValue);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << OutputDevice;
		Ar << OutputGamut;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("CompositeUIPixelShader");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("Main");
	}

private:
	FShaderParameter OutputDevice;
	FShaderParameter OutputGamut;
};

IMPLEMENT_SHADER_TYPE(,FCompositeLUTGenerationPS,TEXT("CompositeUIPixelShader"),TEXT("GenerateLUTPS"),SF_Pixel);

// Pixel shader to composite UI over HDR buffer
template<uint32 EncodingType>
class FCompositePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCompositePS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return SupportsUICompositionRendering(Platform);
	}

	FCompositePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		UITexture.Bind(Initializer.ParameterMap, TEXT("UITexture"));
		UISampler.Bind(Initializer.ParameterMap, TEXT("UISampler"));
		SceneTexture.Bind(Initializer.ParameterMap, TEXT("SceneTexture"));
		SceneSampler.Bind(Initializer.ParameterMap, TEXT("SceneSampler"));
		ColorSpaceLUT.Bind(Initializer.ParameterMap, TEXT("ColorSpaceLUT"));
		ColorSpaceLUTSampler.Bind(Initializer.ParameterMap, TEXT("ColorSpaceLUTSampler"));
		UILevel.Bind(Initializer.ParameterMap, TEXT("UILevel"));
		OutputDevice.Bind(Initializer.ParameterMap, TEXT("OutputDevice"));
	}
	FCompositePS() {}

	void SetParameters(FRHICommandList& RHICmdList, FTextureRHIParamRef UITextureRHI, FTextureRHIParamRef SceneTextureRHI, FTextureRHIParamRef ColorSpaceLUTRHI)
	{
		static const auto CVarOutputDevice = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.Display.OutputDevice"));

		SetTextureParameter(RHICmdList, GetPixelShader(), UITexture, UISampler, TStaticSamplerState<SF_Point>::GetRHI(), UITextureRHI);
		SetTextureParameter(RHICmdList, GetPixelShader(), SceneTexture, SceneSampler, TStaticSamplerState<SF_Point>::GetRHI(), SceneTextureRHI);
		SetTextureParameter(RHICmdList, GetPixelShader(), ColorSpaceLUT, ColorSpaceLUTSampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), ColorSpaceLUTRHI);
		SetShaderValue(RHICmdList, GetPixelShader(), UILevel, CVarUILevel.GetValueOnRenderThread());
		SetShaderValue(RHICmdList, GetPixelShader(), OutputDevice, CVarOutputDevice->GetValueOnRenderThread());
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SCRGB_ENCODING"), EncodingType);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << UITexture << UISampler;
		Ar << SceneTexture << SceneSampler;
		Ar << ColorSpaceLUT <<  ColorSpaceLUTSampler;
		Ar << UILevel << OutputDevice;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("CompositeUIPixelShader");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("Main");
	}

private:
	FShaderResourceParameter UITexture;
	FShaderResourceParameter UISampler;
	FShaderResourceParameter SceneTexture;
	FShaderResourceParameter SceneSampler;
	FShaderResourceParameter ColorSpaceLUT;
	FShaderResourceParameter ColorSpaceLUTSampler;
	FShaderParameter UILevel;
	FShaderParameter OutputDevice;
};

#define SHADER_VARIATION(A) typedef FCompositePS<A> FCompositePS##A; \
	IMPLEMENT_SHADER_TYPE2(FCompositePS##A, SF_Pixel);
SHADER_VARIATION(0)  SHADER_VARIATION(1)
#undef SHADER_VARIATION

/** Draws windows from a FSlateDrawBuffer on the render thread */
void FSlateRHIRenderer::DrawWindow_RenderThread(FRHICommandListImmediate& RHICmdList, FViewportInfo& ViewportInfo, FSlateWindowElementList& WindowElementList, bool bLockToVsync, bool bClear)
{
	SCOPED_DRAW_EVENT(RHICmdList, SlateUI);

	// Should only be called by the rendering thread
	check(IsInRenderingThread());

	// Optional off-screen UI composition during HDR rendering
	static const auto CVarCompositeMode = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.UI.CompositeMode"));
	static const auto CVarHDROutputEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.EnableHDROutput"));

	const bool bSupportsUIComposition = GRHISupportsHDROutput && GSupportsVolumeTextureRendering && SupportsUICompositionRendering(GetFeatureLevelShaderPlatform(GMaxRHIFeatureLevel));
	const bool bCompositeUI = bSupportsUIComposition
		&& CVarCompositeMode && CVarCompositeMode->GetValueOnRenderThread() != 0
		&& CVarHDROutputEnabled && CVarHDROutputEnabled->GetValueOnRenderThread() != 0;

	const int32 CompositionLUTSize = 32;

	// Only need to update LUT on settings change
	static const auto CVarHDROutputDevice = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.Display.OutputDevice"));
	static const auto CVarHDROutputGamut = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.Display.ColorGamut"));

	const int32 HDROutputDevice = CVarHDROutputDevice ? CVarHDROutputDevice->GetValueOnRenderThread() : 0;
	const int32 HDROutputGamut = CVarHDROutputGamut ? CVarHDROutputGamut->GetValueOnRenderThread() : 0;

	bool bLUTStale = ViewportInfo.ColorSpaceLUTOutputDevice != HDROutputDevice || ViewportInfo.ColorSpaceLUTOutputGamut != HDROutputGamut;

	ViewportInfo.ColorSpaceLUTOutputDevice = HDROutputDevice;
	ViewportInfo.ColorSpaceLUTOutputGamut = HDROutputGamut;
	
	bool bRenderedStereo = false;
	if( CVarDrawToVRRenderTarget->GetInt() == 0 && GEngine && IsValidRef( ViewportInfo.GetRenderTargetTexture() ) && GEngine->StereoRenderingDevice.IsValid() )
	{
		GEngine->StereoRenderingDevice->RenderTexture_RenderThread( RHICmdList, RHICmdList.GetViewportBackBuffer( ViewportInfo.ViewportRHI ), ViewportInfo.GetRenderTargetTexture() );
		bRenderedStereo = true;
	}

	{
		SCOPED_GPU_STAT(RHICmdList, Stat_GPU_SlateUI);
		SCOPE_CYCLE_COUNTER( STAT_SlateRenderingRTTime );

		FSlateBatchData& BatchData = WindowElementList.GetBatchData();
		FElementBatchMap& RootBatchMap = WindowElementList.GetRootDrawLayer().GetElementBatchMap();

		WindowElementList.PreDraw_ParallelThread();

		{
			SCOPE_CYCLE_COUNTER(STAT_SlateRTCreateBatches);
			// Update the vertex and index buffer	
			BatchData.CreateRenderBatches(RootBatchMap);
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_SlateRTFillVertexIndexBuffers);
			RenderingPolicy->UpdateVertexAndIndexBuffers(RHICmdList, BatchData);
		}

		// should have been created by the game thread
		check( IsValidRef(ViewportInfo.ViewportRHI) );

		FTexture2DRHIRef ViewportRT = bRenderedStereo ? nullptr : ViewportInfo.GetRenderTargetTexture();
		FTexture2DRHIRef BackBuffer = (ViewportRT) ? ViewportRT : RHICmdList.GetViewportBackBuffer(ViewportInfo.ViewportRHI);
		
		const uint32 ViewportWidth = (ViewportRT) ? ViewportRT->GetSizeX() : ViewportInfo.Width;
		const uint32 ViewportHeight = (ViewportRT) ? ViewportRT->GetSizeY() : ViewportInfo.Height;
		
		// Check to see that targets are up-to-date
		if (bCompositeUI && (!ViewportInfo.UITargetRT || ViewportInfo.UITargetRT->GetSizeX() != ViewportWidth || ViewportInfo.UITargetRT->GetSizeY() != ViewportHeight))
		{
			// Composition buffers
			ViewportInfo.UITargetRT.SafeRelease();
			ViewportInfo.HDRSourceRT.SafeRelease();
			ViewportInfo.ColorSpaceLUTRT.SafeRelease();
			ViewportInfo.UITargetSRV.SafeRelease();

			FRHIResourceCreateInfo CreateInfo;
			RHICreateTargetableShaderResource2D(ViewportWidth, ViewportHeight, PF_B8G8R8A8, 1, TexCreate_None, TexCreate_RenderTargetable, false, CreateInfo, ViewportInfo.UITargetRT, ViewportInfo.UITargetSRV);
			RHICreateTargetableShaderResource2D(ViewportWidth, ViewportHeight, GRHIHDRDisplayOutputFormat, 1, TexCreate_None, TexCreate_RenderTargetable, false, CreateInfo, ViewportInfo.HDRSourceRT, ViewportInfo.HDRSourceSRV);

			// LUT
			ViewportInfo.ColorSpaceLUTRT.SafeRelease();
			ViewportInfo.ColorSpaceLUTSRV.SafeRelease();

			RHICreateTargetableShaderResource3D(CompositionLUTSize, CompositionLUTSize, CompositionLUTSize, PF_A2B10G10R10, 1, TexCreate_None, TexCreate_RenderTargetable, false, CreateInfo, ViewportInfo.ColorSpaceLUTRT, ViewportInfo.ColorSpaceLUTSRV);
			bLUTStale = true;
		}

		FTexture2DRHIRef FinalBuffer = BackBuffer;

		if (bCompositeUI)
		{
			FResolveParams ResolveParams;
			bClear = true; // Force a clear of the UI buffer to black

			// Grab HDR backbuffer
			RHICmdList.CopyToResolveTarget(FinalBuffer, ViewportInfo.HDRSourceRT, false, ResolveParams);

			// UI backbuffer is temp target
			BackBuffer = ViewportInfo.UITargetRT;
		}

		RHICmdList.BeginDrawingViewport( ViewportInfo.ViewportRHI, FTextureRHIRef() );
		RHICmdList.SetViewport(0, 0, 0, ViewportWidth, ViewportHeight, 0.0f);
		RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, BackBuffer);
		if( ViewportInfo.bRequiresStencilTest )
		{
			check(IsValidRef( ViewportInfo.DepthStencil ));

			// Reset the backbuffer as our color render target and also set a depth stencil buffer
			FRHIRenderTargetView ColorView(BackBuffer, 0, -1, bClear ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::EStore);
			FRHISetRenderTargetsInfo Info(1, &ColorView, FRHIDepthRenderTargetView(ViewportInfo.DepthStencil, ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::EStore, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore));

			// Clear the stencil buffer
			RHICmdList.SetRenderTargetsAndClear(Info);
		}
		else
		{
			SetRenderTarget(RHICmdList, BackBuffer, FTextureRHIRef(), bClear ? ESimpleRenderTargetMode::EClearColorAndDepth : ESimpleRenderTargetMode::EExistingColorAndDepth);
		}

#if DEBUG_OVERDRAW
		RHIClear(true, FLinearColor::Black, false, 0.0f, true, 0x00);
#endif
		if( BatchData.GetRenderBatches().Num() > 0 )
		{
			SCOPE_CYCLE_COUNTER(STAT_SlateRTDrawBatches);

			FSlateBackBuffer BackBufferTarget( BackBuffer, FIntPoint( ViewportWidth, ViewportHeight ) );

			RenderingPolicy->DrawElements
			(
				RHICmdList,
				BackBufferTarget,
				ViewMatrix*ViewportInfo.ProjectionMatrix,
				BatchData.GetRenderBatches()
			);
		}

		if (bCompositeUI)
		{
			SCOPED_DRAW_EVENT(RHICmdList, SlateUI_Composition);

			static const FName RendererModuleName("Renderer");
			IRendererModule& RendererModule = FModuleManager::GetModuleChecked<IRendererModule>(RendererModuleName);

			const auto FeatureLevel = GMaxRHIFeatureLevel;
			auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

			// Generate composition LUT
			if (bLUTStale)
			{
				SetRenderTarget(RHICmdList, ViewportInfo.ColorSpaceLUTRT, FTextureRHIRef());

				TShaderMapRef<FWriteToSliceVS> VertexShader(ShaderMap);
				TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(ShaderMap);
				TShaderMapRef<FCompositeLUTGenerationPS> PixelShader(ShaderMap);
				const FVolumeBounds VolumeBounds(CompositionLUTSize);

				static FGlobalBoundShaderState BoundShaderState;
				SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, GScreenVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, *GeometryShader);

				VertexShader->SetParameters(RHICmdList, VolumeBounds, VolumeBounds.MaxX - VolumeBounds.MinX);
				if(GeometryShader.IsValid())
				{
					GeometryShader->SetParameters(RHICmdList, VolumeBounds);
				}
				PixelShader->SetParameters(RHICmdList);

				RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
				RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
				RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
				
				RasterizeToVolumeTexture(RHICmdList, VolumeBounds);

				FResolveParams ResolveParams;
				RHICmdList.CopyToResolveTarget(ViewportInfo.ColorSpaceLUTRT, ViewportInfo.ColorSpaceLUTSRV, false, ResolveParams);
			}

			// Composition pass
			{
				FResolveParams ResolveParams;
				RHICmdList.CopyToResolveTarget(ViewportInfo.UITargetRT, ViewportInfo.UITargetSRV, false, ResolveParams);

				SetRenderTarget(RHICmdList, FinalBuffer, FTextureRHIRef());

				TShaderMapRef<FScreenVS> VertexShader(ShaderMap);

				if (HDROutputDevice == 5 || HDROutputDevice == 6)
				{
					// ScRGB encoding
					TShaderMapRef<FCompositePS<1>> PixelShader(ShaderMap);
					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, RendererModule.GetFilterVertexDeclaration().VertexDeclarationRHI, *VertexShader, *PixelShader);
					PixelShader->SetParameters(RHICmdList, ViewportInfo.UITargetSRV, ViewportInfo.HDRSourceSRV, ViewportInfo.ColorSpaceLUTSRV);
				}
				else
				{
					// ST2084 (PQ) encoding
					TShaderMapRef<FCompositePS<0>> PixelShader(ShaderMap);
					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, RendererModule.GetFilterVertexDeclaration().VertexDeclarationRHI, *VertexShader, *PixelShader);
					PixelShader->SetParameters(RHICmdList, ViewportInfo.UITargetSRV, ViewportInfo.HDRSourceSRV, ViewportInfo.ColorSpaceLUTSRV);
				}

				RendererModule.DrawRectangle(
					RHICmdList,
					0, 0,
					ViewportWidth, ViewportHeight,
					0, 0,
					ViewportWidth, ViewportHeight,
					FIntPoint(ViewportWidth, ViewportHeight),
					FIntPoint(ViewportWidth, ViewportHeight),
					*VertexShader,
					EDRF_UseTriangleOptimization);
			}
		}
	}

	if (!bRenderedStereo && GEngine && IsValidRef(ViewportInfo.GetRenderTargetTexture()) && GEngine->StereoRenderingDevice.IsValid())
	{
		GEngine->StereoRenderingDevice->RenderTexture_RenderThread(RHICmdList, RHICmdList.GetViewportBackBuffer(ViewportInfo.ViewportRHI), ViewportInfo.GetRenderTargetTexture());
	}

	// Calculate renderthread time (excluding idle time).	
	uint32 StartTime		= FPlatformTime::Cycles();

	RHICmdList.EndDrawingViewport(ViewportInfo.ViewportRHI, true, bLockToVsync);

	uint32 EndTime		= FPlatformTime::Cycles();

	GSwapBufferTime		= EndTime - StartTime;
	SET_CYCLE_COUNTER(STAT_PresentTime, GSwapBufferTime);

	static uint32 LastTimestamp	= 0;
	uint32 ThreadTime	= EndTime - LastTimestamp;
	LastTimestamp		= EndTime;

	uint32 RenderThreadIdle = 0;	

	FThreadIdleStats& RenderThread = FThreadIdleStats::Get();
	GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForAllOtherSleep] = RenderThread.Waits;
	GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUPresent] += GSwapBufferTime;
	GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUPresent]++;
	RenderThread.Waits = 0;

	SET_CYCLE_COUNTER(STAT_RenderingIdleTime_RenderThreadSleepTime, GRenderThreadIdle[0]);
	SET_CYCLE_COUNTER(STAT_RenderingIdleTime_WaitingForGPUQuery, GRenderThreadIdle[1]);
	SET_CYCLE_COUNTER(STAT_RenderingIdleTime_WaitingForGPUPresent, GRenderThreadIdle[2]);

	for (int32 Index = 0; Index < ERenderThreadIdleTypes::Num; Index++)
	{
		RenderThreadIdle += GRenderThreadIdle[Index];
		GRenderThreadIdle[Index] = 0;
		GRenderThreadNumIdle[Index] = 0;
	}

	SET_CYCLE_COUNTER(STAT_RenderingIdleTime, RenderThreadIdle);	
	GRenderThreadTime	= (ThreadTime > RenderThreadIdle) ? (ThreadTime - RenderThreadIdle) : ThreadTime;
}

void FSlateRHIRenderer::DrawWindows( FSlateDrawBuffer& WindowDrawBuffer )
{
	if (IsInSlateThread())
	{
		EnqueuedWindowDrawBuffer = &WindowDrawBuffer;
	}
	else
	{
		DrawWindows_Private(WindowDrawBuffer);
	}
}

void FSlateRHIRenderer::DrawWindows()
{
	if (EnqueuedWindowDrawBuffer)
	{
		DrawWindows_Private(*EnqueuedWindowDrawBuffer);
		EnqueuedWindowDrawBuffer = NULL;
	}
}

void FSlateRHIRenderer::PrepareToTakeScreenshot(const FIntRect& Rect, TArray<FColor>* OutColorData)
{
	check(OutColorData);

	bTakingAScreenShot = true;
	ScreenshotRect = Rect;
	OutScreenshotData = OutColorData;
}

/** 
 * Creates necessary resources to render a window and sends draw commands to the rendering thread
 *
 * @param WindowDrawBuffer	The buffer containing elements to draw 
 */
void FSlateRHIRenderer::DrawWindows_Private( FSlateDrawBuffer& WindowDrawBuffer )
{
	checkSlow( IsThreadSafeForSlateRendering() );

	// Enqueue a command to unlock the draw buffer after all windows have been drawn
	FSlateRHIRenderingPolicy* Policy = RenderingPolicy.Get();
	ENQUEUE_RENDER_COMMAND(SlateBeginDrawingWindowsCommand)(
		[Policy](FRHICommandListImmediate& RHICmdList)
		{
			Policy->BeginDrawingWindows();
		}
	);

	// Update texture atlases if needed and safe
	if (DoesThreadOwnSlateRendering())
	{
		ResourceManager->UpdateTextureAtlases();
	}

	const TSharedRef<FSlateFontCache> FontCache = SlateFontServices->GetFontCache();

	// Iterate through each element list and set up an RHI window for it if needed
	TArray<TSharedPtr<FSlateWindowElementList>>& WindowElementLists = WindowDrawBuffer.GetWindowElementLists();
	for( int32 ListIndex = 0; ListIndex < WindowElementLists.Num(); ++ListIndex )
	{
		FSlateWindowElementList& ElementList = *WindowElementLists[ListIndex];

		TSharedPtr<SWindow> Window = ElementList.GetWindow();

		if( Window.IsValid() )
		{
			const FVector2D WindowSize = Window->GetViewportSize();
			if ( WindowSize.X > 0 && WindowSize.Y > 0 )
			{
				// Add all elements for this window to the element batcher
				ElementBatcher->AddElements( ElementList );

				// Update the font cache with new text after elements are batched
				FontCache->UpdateCache();

				bool bRequiresStencilTest = false;
				bool bLockToVsync = false;

				bLockToVsync = ElementBatcher->RequiresVsync();

				bool bForceVsyncFromCVar = false;
				if(GIsEditor)
				{
					static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSyncEditor"));
					bForceVsyncFromCVar = (CVar->GetInt() != 0);
				}
				else
				{
					static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
					bForceVsyncFromCVar = (CVar->GetInt() != 0);
				}

				bLockToVsync |= bForceVsyncFromCVar;

				// All elements for this window have been batched and rendering data updated
				ElementBatcher->ResetBatches();

				// The viewport had better exist at this point  
				FViewportInfo* ViewInfo = WindowToViewportInfo.FindChecked( Window.Get() );

				if (Window->IsViewportSizeDrivenByWindow())
				{
					// Resize the viewport if needed
					ConditionalResizeViewport(ViewInfo, ViewInfo->DesiredWidth, ViewInfo->DesiredHeight, IsViewportFullscreen(*Window));
				}

				if( bRequiresStencilTest )
				{	
					ViewInfo->ConditionallyUpdateDepthBuffer(bRequiresStencilTest);
				}

				// Tell the rendering thread to draw the windows
				{
					struct FSlateDrawWindowCommandParams
					{
						FSlateRHIRenderer* Renderer;
						FSlateRHIRenderer::FViewportInfo* ViewportInfo;
						FSlateWindowElementList* WindowElementList;
						SWindow* SlateWindow;
						bool bLockToVsync;
						bool bClear;
					} Params;

					Params.Renderer = this;
					Params.ViewportInfo = ViewInfo;
					Params.WindowElementList = &ElementList;
					Params.bLockToVsync = bLockToVsync;
#if ALPHA_BLENDED_WINDOWS
					Params.bClear = Window->GetTransparencySupport() == EWindowTransparency::PerPixel;
#else
					Params.bClear = false;
#endif

					// NOTE: We pass a raw pointer to the SWindow so that we don't have to use a thread-safe weak pointer in
					// the FSlateWindowElementList structure
					Params.SlateWindow = Window.Get();

					// Skip the actual draw if we're in a headless execution environment
					if (GIsClient && !IsRunningCommandlet() && !GUsingNullRHI)
					{
						ENQUEUE_RENDER_COMMAND(SlateDrawWindowsCommand)(
							[Params](FRHICommandListImmediate& RHICmdList)
							{
								Params.Renderer->DrawWindow_RenderThread(RHICmdList, *Params.ViewportInfo, *Params.WindowElementList, Params.bLockToVsync, Params.bClear);
							}
						);
					}

					SlateWindowRendered.Broadcast( *Params.SlateWindow, &ViewInfo->ViewportRHI );

					if ( bTakingAScreenShot )
					{
						FIntRect LocalScreenshotRect = ScreenshotRect;
						TArray<FColor>* LocalOutScreenshotData = OutScreenshotData;
						ENQUEUE_RENDER_COMMAND(SlateCaptureScreenshotCommand)(
							[Params, LocalScreenshotRect, LocalOutScreenshotData](FRHICommandListImmediate& RHICmdList)
							{
								FTexture2DRHIRef BackBuffer = RHICmdList.GetViewportBackBuffer(Params.ViewportInfo->ViewportRHI);
								RHICmdList.ReadSurfaceData(BackBuffer, LocalScreenshotRect, *LocalOutScreenshotData, FReadSurfaceDataFlags());
							}
						);

						FlushRenderingCommands();

						bTakingAScreenShot = false;
						OutScreenshotData = NULL;
					}
				}
			}
		}
		else
		{
			ensureMsgf( false, TEXT("Window isnt valid but being drawn!") );
		}
	}

	FSlateDrawBuffer* DrawBuffer = &WindowDrawBuffer;
	ENQUEUE_RENDER_COMMAND(SlateEndDrawingWindowsCommand)(
		[DrawBuffer, Policy](FRHICommandListImmediate& RHICmdList)
		{
			FSlateEndDrawingWindowsCommand::EndDrawingWindows(RHICmdList, DrawBuffer, *Policy);
		}
	);

	// flush the cache if needed
	FontCache->ConditionalFlushCache();
}


FIntRect FSlateRHIRenderer::SetupVirtualScreenBuffer(const bool bPrimaryWorkAreaOnly, const float ScreenScaling, ILiveStreamingService* LiveStreamingService )
{
	// Figure out how big we need our render targets to be, based on the size of the entire desktop and the configured scaling amount
	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetDisplayMetrics(DisplayMetrics);

#if !PLATFORM_WINDOWS && !PLATFORM_MAC && !PLATFORM_LINUX
	ensureMsgf(0, TEXT("This functionality is not valid for this platform"));	
	return FIntRect(FIntPoint(0, 0), FIntPoint(DisplayMetrics.PrimaryDisplayWidth, DisplayMetrics.PrimaryDisplayHeight));
#endif	

	FIntPoint UnscaledVirtualScreenOrigin;
	FIntPoint UnscaledVirtualScreenLowerRight;

	if( bPrimaryWorkAreaOnly )
	{
		UnscaledVirtualScreenOrigin = FIntPoint(DisplayMetrics.PrimaryDisplayWorkAreaRect.Left, DisplayMetrics.PrimaryDisplayWorkAreaRect.Top);
		UnscaledVirtualScreenLowerRight = FIntPoint(DisplayMetrics.PrimaryDisplayWorkAreaRect.Right, DisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom);
	}
	else
	{
		UnscaledVirtualScreenOrigin = FIntPoint(DisplayMetrics.VirtualDisplayRect.Left, DisplayMetrics.VirtualDisplayRect.Top);
		UnscaledVirtualScreenLowerRight = FIntPoint(DisplayMetrics.VirtualDisplayRect.Right, DisplayMetrics.VirtualDisplayRect.Bottom);
	}
	const FIntRect UnscaledVirtualScreen = FIntRect(UnscaledVirtualScreenOrigin, UnscaledVirtualScreenLowerRight);
	FIntRect ScaledVirtualScreen;
	{
		int BufferWidth = FMath::FloorToInt( (float)UnscaledVirtualScreen.Width() * ScreenScaling );
		int BufferHeight = FMath::FloorToInt( (float)UnscaledVirtualScreen.Height() * ScreenScaling );
		
		// If we're preparing a buffer for live streaming, then go ahead and make sure the resolution will be valid for that
		if( LiveStreamingService != nullptr )
		{
			// @todo livestream: This could cause the aspect ratio to be changed and the buffer to be stretched non-uniformly, but usually the aspect only changes slightly
			LiveStreamingService->MakeValidVideoBufferResolution( BufferWidth, BufferHeight );
		}

		const float XScaling = (float)BufferWidth / (float)UnscaledVirtualScreen.Width();
		const float YScaling = (float)BufferHeight / (float)UnscaledVirtualScreen.Height();

		ScaledVirtualScreen.Min.X = FMath::FloorToInt(( float)UnscaledVirtualScreen.Min.X * XScaling );
		ScaledVirtualScreen.Max.X = FMath::FloorToInt(( float)UnscaledVirtualScreen.Max.X * XScaling );
		ScaledVirtualScreen.Min.Y = FMath::FloorToInt(( float)UnscaledVirtualScreen.Min.Y * YScaling );
		ScaledVirtualScreen.Max.Y = FMath::FloorToInt(( float)UnscaledVirtualScreen.Max.Y * YScaling );
	}

	// @todo livestream: This CrashTrackerResource is now also used for editor live streaming, so we should consider renaming it and cleaning
	// up the API a little bit more
	if( CrashTrackerResource == nullptr || 
		ScaledVirtualScreen != CrashTrackerResource->GetVirtualScreen() )
	{
		if( CrashTrackerResource != nullptr )
		{
			// Size has changed, so clear out our old resource and create a new one
			BeginReleaseResource(CrashTrackerResource);

			FlushRenderingCommands();
	
			if (CrashTrackerResource != nullptr)
			{
				delete CrashTrackerResource;
				CrashTrackerResource = NULL;
			}
		}

		CrashTrackerResource = new FSlateCrashReportResource(ScaledVirtualScreen, UnscaledVirtualScreen);
		BeginInitResource(CrashTrackerResource);
	}

	return CrashTrackerResource->GetVirtualScreen();
}


void FSlateRHIRenderer::CopyWindowsToVirtualScreenBuffer(const TArray<FString>& KeypressBuffer)
{
#if !PLATFORM_WINDOWS && !PLATFORM_MAC && !PLATFORM_LINUX
	ensureMsgf(0, TEXT("This functionality is not valid for this platform"));
	return;
#endif

	SCOPE_CYCLE_COUNTER(STAT_GenerateCaptureBuffer);

	// Make sure to call SetupDrawBuffer() before calling this function!
	check( CrashTrackerResource != nullptr );

	const FIntRect VirtualScreen = CrashTrackerResource->GetVirtualScreen();
	const FIntPoint VirtualScreenPos = VirtualScreen.Min;
	const FIntPoint VirtualScreenSize = VirtualScreen.Size();
	const FIntRect UnscaledVirtualScreen = CrashTrackerResource->GetUnscaledVirtualScreen();
	const float XScaling = (float)VirtualScreen.Width() / (float)UnscaledVirtualScreen.Width();
	const float YScaling = (float)VirtualScreen.Height() / (float)UnscaledVirtualScreen.Height();
	
	// setup state
	struct FSetupWindowStateContext
	{
		FSlateCrashReportResource* CrashReportResource;
		FIntRect IntermediateBufferSize;
	};
	FSetupWindowStateContext SetupWindowStateContext =
	{
		CrashTrackerResource,
		VirtualScreen
	};

	ENQUEUE_RENDER_COMMAND(SetupWindowState)(
		[SetupWindowStateContext](FRHICommandListImmediate& RHICmdList)
		{
			SetRenderTarget(RHICmdList, SetupWindowStateContext.CrashReportResource->GetBuffer(), FTextureRHIRef());
			RHICmdList.SetViewport(0, 0, 0.0f, SetupWindowStateContext.IntermediateBufferSize.Width(), SetupWindowStateContext.IntermediateBufferSize.Height(), 1.0f);
			RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
			RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
		
			RHICmdList.SetViewport(0, 0, 0.0f, SetupWindowStateContext.IntermediateBufferSize.Width(), SetupWindowStateContext.IntermediateBufferSize.Height(), 1.0f);
			RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
			RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

			// @todo livestream: Ideally this "desktop background color" should be configurable in the editor's preferences
			RHICmdList.ClearColorTexture(SetupWindowStateContext.CrashReportResource->GetBuffer(), FLinearColor(0.8f, 0.00f, 0.0f));
		}
	);

	// draw windows to buffer
	TArray< TSharedRef<SWindow> > OutWindows;
	FSlateApplication::Get().GetAllVisibleWindowsOrdered(OutWindows);

	static const FName RendererModuleName( "Renderer" );
	IRendererModule& RendererModule = FModuleManager::GetModuleChecked<IRendererModule>( RendererModuleName );

	//if ( false )
	{
		for (int32 i = 0; i < OutWindows.Num(); ++i)
		{
			TSharedPtr<SWindow> WindowPtr = OutWindows[i];
			SWindow* Window = WindowPtr.Get();
			FViewportInfo* ViewportInfo = WindowToViewportInfo.FindChecked(Window);

			const FSlateRect SlateWindowRect = Window->GetRectInScreen();
			const FVector2D WindowSize = SlateWindowRect.GetSize();
			if ( WindowSize.X > 0 && WindowSize.Y > 0 )
			{
				FIntRect ScaledWindowRect;
				ScaledWindowRect.Min.X = SlateWindowRect.Left * XScaling - VirtualScreenPos.X;
				ScaledWindowRect.Max.X = SlateWindowRect.Right * XScaling - VirtualScreenPos.X;
				ScaledWindowRect.Min.Y = SlateWindowRect.Top * YScaling - VirtualScreenPos.Y;
				ScaledWindowRect.Max.Y = SlateWindowRect.Bottom * YScaling - VirtualScreenPos.Y;

				struct FDrawWindowToBufferContext
				{
					FViewportInfo* InViewportInfo;
					FIntRect WindowRect;
					FIntRect IntermediateBufferSize;
					IRendererModule* RendererModule;
				};
				FDrawWindowToBufferContext DrawWindowToBufferContext =
				{
					ViewportInfo,
					ScaledWindowRect,
					VirtualScreen,
					&RendererModule
				};

				// Draw a quad mapping scene color to the view's render target
				ENQUEUE_RENDER_COMMAND(DrawWindowToBuffer)(
					[DrawWindowToBufferContext](FRHICommandListImmediate& RHICmdList)
					{
						const auto FeatureLevel = GMaxRHIFeatureLevel;
						auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

						TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
						TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

						static FGlobalBoundShaderState BoundShaderState;
						SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, DrawWindowToBufferContext.RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI, *VertexShader, *PixelShader);

						if(DrawWindowToBufferContext.WindowRect.Width() != DrawWindowToBufferContext.InViewportInfo->Width || DrawWindowToBufferContext.WindowRect.Height() != DrawWindowToBufferContext.InViewportInfo->Height )
						{
							// We're scaling down the window, so use bilinear filtering
							PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), RHICmdList.GetViewportBackBuffer(DrawWindowToBufferContext.InViewportInfo->ViewportRHI));
						}
						else
						{
							// Drawing 1:1, so no filtering needed
							PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), RHICmdList.GetViewportBackBuffer(DrawWindowToBufferContext.InViewportInfo->ViewportRHI));
						}

						DrawWindowToBufferContext.RendererModule->DrawRectangle(
							RHICmdList,
							DrawWindowToBufferContext.WindowRect.Min.X, DrawWindowToBufferContext.WindowRect.Min.Y,
							DrawWindowToBufferContext.WindowRect.Width(), DrawWindowToBufferContext.WindowRect.Height(),
							0, 0,
							1, 1,
							FIntPoint(DrawWindowToBufferContext.IntermediateBufferSize.Width(), DrawWindowToBufferContext.IntermediateBufferSize.Height()),
							FIntPoint(1, 1),
							*VertexShader,
							EDRF_Default);
					}
				);
			}
		}
	}

	// draw mouse cursor and keypresses
	const FVector2D MouseCursorLocation = FSlateApplication::Get().GetCursorPos();
	const FIntPoint ScaledCursorLocation = FIntPoint(MouseCursorLocation.X * XScaling, MouseCursorLocation.Y * YScaling) - VirtualScreenPos;

	FSlateWindowElementList* WindowElementList = CrashTrackerResource->GetNextElementList();
	WindowElementList->ResetBuffers();

	// Don't draw cursor when it is hidden (mouse looking, scrolling, etc.)
	// @todo livestream: The cursor is probably still hidden when dragging with the mouse captured (grabby hand)
	if( FSlateApplication::Get().GetMouseCaptureWindow() == nullptr )
	{
		FSlateDrawElement::MakeBox(
			*WindowElementList,
			0,
			FPaintGeometry(ScaledCursorLocation, FVector2D(32, 32) * XScaling, XScaling),
			FCoreStyle::Get().GetBrush("CrashTracker.Cursor"),
			FSlateRect(0, 0, VirtualScreenSize.X, VirtualScreenSize.Y));
	}
	
	for (int32 i = 0; i < KeypressBuffer.Num(); ++i)
	{
		FSlateDrawElement::MakeText(
			*WindowElementList,
			0,
			FPaintGeometry(FVector2D(10, 10 + i * 30), FVector2D(300, 30), 1.f),
			KeypressBuffer[i],
			FCoreStyle::Get().GetFontStyle(TEXT("CrashTracker.Font")),
			FSlateRect(0, 0, VirtualScreenSize.X, VirtualScreenSize.Y));
	}
	
	ElementBatcher->AddElements(*WindowElementList);
	ElementBatcher->ResetBatches();
	
	struct FWriteMouseCursorAndKeyPressesContext
	{
		FSlateCrashReportResource* CrashReportResource;
		FIntRect IntermediateBufferSize;
		FSlateRHIRenderingPolicy* RenderPolicy;
		FSlateWindowElementList* SlateElementList;
		FIntPoint ViewportSize;
	};
	FWriteMouseCursorAndKeyPressesContext WriteMouseCursorAndKeyPressesContext =
	{
		CrashTrackerResource,
		VirtualScreen,
		RenderingPolicy.Get(),
		WindowElementList,
		VirtualScreenSize
	};
	ENQUEUE_RENDER_COMMAND(WriteMouseCursorAndKeyPresses)(
		[WriteMouseCursorAndKeyPressesContext](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI());
		
			FSlateBatchData& BatchData = WriteMouseCursorAndKeyPressesContext.SlateElementList->GetBatchData();
			FElementBatchMap& RootBatchMap = WriteMouseCursorAndKeyPressesContext.SlateElementList->GetRootDrawLayer().GetElementBatchMap();

			BatchData.CreateRenderBatches(RootBatchMap);

			WriteMouseCursorAndKeyPressesContext.RenderPolicy->UpdateVertexAndIndexBuffers(RHICmdList, BatchData);
			if( BatchData.GetRenderBatches().Num() > 0 )
			{
				FTexture2DRHIRef UnusedTargetTexture;
				FSlateBackBuffer UnusedTarget( UnusedTargetTexture, FIntPoint::ZeroValue );

				WriteMouseCursorAndKeyPressesContext.RenderPolicy->DrawElements(RHICmdList, UnusedTarget, CreateProjectionMatrix(WriteMouseCursorAndKeyPressesContext.ViewportSize.X, WriteMouseCursorAndKeyPressesContext.ViewportSize.Y), BatchData.GetRenderBatches());
			}
		}
	);

	// copy back to the cpu
	struct FReadbackFromIntermediateBufferContext
	{
		FSlateCrashReportResource* CrashReportResource;
		FIntRect IntermediateBufferSize;
	};
	FReadbackFromIntermediateBufferContext ReadbackFromIntermediateBufferContext =
	{
		CrashTrackerResource,
		VirtualScreen
	};
	ENQUEUE_RENDER_COMMAND(ReadbackFromIntermediateBuffer)(
		[ReadbackFromIntermediateBufferContext](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.CopyToResolveTarget(
				ReadbackFromIntermediateBufferContext.CrashReportResource->GetBuffer(),
				ReadbackFromIntermediateBufferContext.CrashReportResource->GetReadbackBuffer(),
				false,
				FResolveParams());
		}
	);
}


void FSlateRHIRenderer::MapVirtualScreenBuffer(FMappedTextureBuffer* OutTextureData)
{
	const FIntRect VirtualScreen = CrashTrackerResource->GetVirtualScreen();

	struct FReadbackFromStagingBufferContext
	{
		FSlateCrashReportResource* CrashReportResource;
		FMappedTextureBuffer* TextureData;
		FIntRect ExpectedBufferSize;
	};
	FReadbackFromStagingBufferContext ReadbackFromStagingBufferContext =
	{
		CrashTrackerResource,
		OutTextureData,
		VirtualScreen,
	};
	ENQUEUE_RENDER_COMMAND(ReadbackFromStagingBuffer)(
		[ReadbackFromStagingBufferContext](FRHICommandListImmediate& RHICmdList)
		{
			SCOPE_CYCLE_COUNTER(STAT_MapStagingBuffer);
			RHICmdList.MapStagingSurface(ReadbackFromStagingBufferContext.CrashReportResource->GetReadbackBuffer(), ReadbackFromStagingBufferContext.TextureData->Data, ReadbackFromStagingBufferContext.TextureData->Width, ReadbackFromStagingBufferContext.TextureData->Height);

			ReadbackFromStagingBufferContext.CrashReportResource->SwapTargetReadbackBuffer();
		}
	);
}

void FSlateRHIRenderer::UnmapVirtualScreenBuffer()
{
	struct FReadbackFromStagingBufferContext
	{
		FSlateCrashReportResource* CrashReportResource;
	};
	FReadbackFromStagingBufferContext ReadbackFromStagingBufferContext =
	{
		CrashTrackerResource
	};
	ENQUEUE_RENDER_COMMAND(ReadbackFromStagingBuffer)(
		[ReadbackFromStagingBufferContext](FRHICommandListImmediate& RHICmdList)
		{
			SCOPE_CYCLE_COUNTER(STAT_UnmapStagingBuffer);
			RHICmdList.UnmapStagingSurface(ReadbackFromStagingBufferContext.CrashReportResource->GetReadbackBuffer());
		}
	);
}

FIntPoint FSlateRHIRenderer::GenerateDynamicImageResource(const FName InTextureName)
{
	check( IsInGameThread() );

	uint32 Width = 0;
	uint32 Height = 0;
	TArray<uint8> RawData;

	TSharedPtr<FSlateDynamicTextureResource> TextureResource = ResourceManager->GetDynamicTextureResourceByName( InTextureName );
	if( !TextureResource.IsValid() )
	{
		// Load the image from disk
		bool bSucceeded = ResourceManager->LoadTexture(InTextureName, InTextureName.ToString(), Width, Height, RawData);
		if (bSucceeded)
		{
			TextureResource = ResourceManager->MakeDynamicTextureResource(InTextureName, Width, Height, RawData);
		}
	}

	return TextureResource.IsValid() ? TextureResource->Proxy->ActualSize : FIntPoint( 0, 0 );
}

bool FSlateRHIRenderer::GenerateDynamicImageResource( FName ResourceName, uint32 Width, uint32 Height, const TArray< uint8 >& Bytes )
{
	check( IsInGameThread() );

	TSharedPtr<FSlateDynamicTextureResource> TextureResource = ResourceManager->GetDynamicTextureResourceByName( ResourceName );
	if( !TextureResource.IsValid() )
	{
		TextureResource = ResourceManager->MakeDynamicTextureResource( ResourceName, Width, Height, Bytes );
	}
	return TextureResource.IsValid();
}

bool FSlateRHIRenderer::GenerateDynamicImageResource(FName ResourceName, FSlateTextureDataRef TextureData)
{
	check(IsInGameThread());

	TSharedPtr<FSlateDynamicTextureResource> TextureResource = ResourceManager->GetDynamicTextureResourceByName(ResourceName);
	if ( !TextureResource.IsValid() )
	{
		TextureResource = ResourceManager->MakeDynamicTextureResource(ResourceName, TextureData);
	}
	return TextureResource.IsValid();
}

FSlateResourceHandle FSlateRHIRenderer::GetResourceHandle( const FSlateBrush& Brush )
{
	return ResourceManager->GetResourceHandle( Brush );
}

void FSlateRHIRenderer::RemoveDynamicBrushResource( TSharedPtr<FSlateDynamicImageBrush> BrushToRemove )
{
	if( BrushToRemove.IsValid() )
	{
		DynamicBrushesToRemove[FreeBufferIndex].Add( BrushToRemove );
	}
}

/**
 * Gives the renderer a chance to wait for any render commands to be completed before returning/
 */
void FSlateRHIRenderer::FlushCommands() const
{
	if( IsInGameThread() )
	{
		FlushRenderingCommands();
	}
}

/**
 * Gives the renderer a chance to synchronize with another thread in the event that the renderer runs 
 * in a multi-threaded environment.  This function does not return until the sync is complete
 */
void FSlateRHIRenderer::Sync() const
{
	// Sync game and render thread. Either total sync or allowing one frame lag.
	static FFrameEndSync FrameEndSync;
	static auto CVarAllowOneFrameThreadLag = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.OneFrameThreadLag"));
	FrameEndSync.Sync( CVarAllowOneFrameThreadLag->GetValueOnAnyThread() != 0 );
}

void FSlateRHIRenderer::ReloadTextureResources()
{
	ResourceManager->ReloadTextures();
}

void FSlateRHIRenderer::LoadUsedTextures()
{
	if ( ResourceManager.IsValid() )
	{
		ResourceManager->LoadUsedTextures();
	}
}

void FSlateRHIRenderer::LoadStyleResources( const ISlateStyle& Style )
{
	if ( ResourceManager.IsValid() )
	{
		ResourceManager->LoadStyleResources( Style );
	}
}

void FSlateRHIRenderer::ReleaseDynamicResource( const FSlateBrush& InBrush )
{
	ensure( IsInGameThread() );
	ResourceManager->ReleaseDynamicResource( InBrush );
}

void* FSlateRHIRenderer::GetViewportResource( const SWindow& Window )
{
	checkSlow(IsThreadSafeForSlateRendering());

	FViewportInfo** InfoPtr = WindowToViewportInfo.Find( &Window );

	if( InfoPtr )
	{
		FViewportInfo* ViewportInfo = *InfoPtr;

		// Create the viewport if it doesnt exist
		if( !IsValidRef(ViewportInfo->ViewportRHI) )
		{
			// Sanity check dimensions
			checkf(ViewportInfo->Width <= MAX_VIEWPORT_SIZE && ViewportInfo->Height <= MAX_VIEWPORT_SIZE, TEXT("Invalid window with Width=%u and Height=%u"), ViewportInfo->Width, ViewportInfo->Height);

			const bool bFullscreen = IsViewportFullscreen( Window );

			ViewportInfo->ViewportRHI = RHICreateViewport( ViewportInfo->OSWindow, ViewportInfo->Width, ViewportInfo->Height, bFullscreen, ViewportInfo->PixelFormat );
		}

		return &ViewportInfo->ViewportRHI;
	}
	else
	{
		return NULL;
	}
}

void FSlateRHIRenderer::SetColorVisionDeficiencyType( uint32 Type )
{
	GSlateShaderColorVisionDeficiencyType = Type;
}

FSlateUpdatableTexture* FSlateRHIRenderer::CreateUpdatableTexture(uint32 Width, uint32 Height)
{
	const bool bCreateEmptyTexture = true;
	FSlateTexture2DRHIRef* NewTexture = new FSlateTexture2DRHIRef(Width, Height, PF_B8G8R8A8, nullptr, TexCreate_Dynamic, bCreateEmptyTexture);
	if (IsInRenderingThread())
	{
		NewTexture->InitResource();
	}
	else
	{
		BeginInitResource(NewTexture);
	}
	return NewTexture;
}

void FSlateRHIRenderer::ReleaseUpdatableTexture(FSlateUpdatableTexture* Texture)
{
	if (IsInRenderingThread())
	{
		Texture->GetRenderResource()->ReleaseResource();
		delete Texture;
	}
	else
	{
		Texture->Cleanup();
	}
}

ISlateAtlasProvider* FSlateRHIRenderer::GetTextureAtlasProvider()
{
	if( ResourceManager.IsValid() )
	{
		return ResourceManager->GetTextureAtlasProvider();
	}

	return nullptr;
}



int32 FSlateRHIRenderer::RegisterCurrentScene(FSceneInterface* Scene)
{
	check(IsInGameThread());
	if (Scene)
	{
		CurrentSceneIndex = ActiveScenes.AddUnique(Scene);
	}
	else
	{
		CurrentSceneIndex = -1;
	}

	// We need to keep the ActiveScenes array synchronized with the Policy's ActiveScenes array on
	// the render thread.
	FSlateRHIRenderingPolicy* InRenderPolicy = RenderingPolicy.Get();
	int32 LocalCurrentSceneIndex = CurrentSceneIndex;
	ENQUEUE_RENDER_COMMAND(RegisterCurrentSceneOnPolicy)(
		[InRenderPolicy, Scene, LocalCurrentSceneIndex](FRHICommandListImmediate& RHICmdList)
		{
			if (LocalCurrentSceneIndex != -1)
			{
				InRenderPolicy->AddSceneAt(Scene, LocalCurrentSceneIndex);
			}
		}
	);
	return CurrentSceneIndex;
}

int32 FSlateRHIRenderer::GetCurrentSceneIndex() const
{
	return CurrentSceneIndex;
}

void FSlateRHIRenderer::ClearScenes()
{
	if(!IsInSlateThread())
	{
		CurrentSceneIndex = -1;
		ActiveScenes.Empty();

		// We need to keep the ActiveScenes array synchronized with the Policy's ActiveScenes array on
		// the render thread.
		FSlateRenderingPolicy* InRenderPolicy = RenderingPolicy.Get();
		ENQUEUE_RENDER_COMMAND(ClearScenesOnPolicy)(
			[InRenderPolicy](FRHICommandListImmediate& RHICmdList)
			{
				InRenderPolicy->ClearScenes();
			}
		);
	}
}

bool FSlateRHIRenderer::AreShadersInitialized() const
{
#if WITH_EDITORONLY_DATA
	return IsGlobalShaderMapComplete(TEXT("SlateElement"));
#else
	return true;
#endif
}

void FSlateRHIRenderer::InvalidateAllViewports()
{
	for( TMap< const SWindow*, FViewportInfo*>::TIterator It(WindowToViewportInfo); It; ++It )
	{
		It.Value()->ViewportRHI = nullptr;
	}
}

void FSlateRHIRenderer::ReleaseAccessedResources(bool bImmediatelyFlush)
{
	FScopeLock ScopeLock(GetResourceCriticalSection());

	// Clear accessed UTexture and Material objects from the previous frame
	ResourceManager->BeginReleasingAccessedResources(bImmediatelyFlush);

	// We keep track of the Scene objects from SceneViewports on the SlateRenderer. Make sure that this gets refreshed every frame.
	ClearScenes();

	if ( bImmediatelyFlush )
	{
		// Release resources generated specifically by the rendering policy if we are flushing.  This should NOT be done unless flushing
		RenderingPolicy->FlushGeneratedResources();

		FlushCommands();
	}
}

void FSlateRHIRenderer::RequestResize( const TSharedPtr<SWindow>& Window, uint32 NewWidth, uint32 NewHeight )
{
	checkSlow( IsThreadSafeForSlateRendering() );

	FViewportInfo* ViewInfo = WindowToViewportInfo.FindRef( Window.Get() );

	if (ViewInfo)
	{
		ViewInfo->DesiredWidth = NewWidth;
		ViewInfo->DesiredHeight = NewHeight;
	}
}

void FSlateRHIRenderer::SetWindowRenderTarget(const SWindow& Window, IViewportRenderTargetProvider* Provider)
{	
	FViewportInfo* ViewInfo = WindowToViewportInfo.FindRef(&Window);
	if (ViewInfo)
	{
		ViewInfo->RTProvider = Provider;
	}
}

TSharedRef<FSlateRenderDataHandle, ESPMode::ThreadSafe> FSlateRHIRenderer::CacheElementRenderData(const ILayoutCache* Cacher, FSlateWindowElementList& ElementList)
{
	TSharedRef<FSlateRenderDataHandle, ESPMode::ThreadSafe> RenderDataHandle = MakeShareable(new FSlateRenderDataHandle(Cacher, ResourceManager.Get()));

	checkSlow(ElementList.GetChildDrawLayers().Num() == 0);

	// Add all elements for this window to the element batcher
	ElementBatcher->AddElements(ElementList);

	// All elements for this window have been batched and rendering data updated
	ElementBatcher->ResetBatches();

	struct FCacheElementBatchesContext
	{
		FSlateRHIRenderingPolicy* RenderPolicy;
		FSlateWindowElementList* SlateElementList;
		TSharedRef<FSlateRenderDataHandle, ESPMode::ThreadSafe> RenderDataHandle;
	};
	FCacheElementBatchesContext CacheElementBatchesContext =
	{
		RenderingPolicy.Get(),
		&ElementList,
		RenderDataHandle,
	};
	ENQUEUE_RENDER_COMMAND(CacheElementBatches)(
		[CacheElementBatchesContext](FRHICommandListImmediate& RHICmdList)
		{
			FSlateBatchData& BatchData = CacheElementBatchesContext.SlateElementList->GetBatchData();
			FElementBatchMap& RootBatchMap = CacheElementBatchesContext.SlateElementList->GetRootDrawLayer().GetElementBatchMap();

			BatchData.SetRenderDataHandle(CacheElementBatchesContext.RenderDataHandle);
			BatchData.CreateRenderBatches(RootBatchMap);
			CacheElementBatchesContext.RenderPolicy->UpdateVertexAndIndexBuffers(RHICmdList, BatchData, CacheElementBatchesContext.RenderDataHandle);
		}
	);

	return RenderDataHandle;
}

void FSlateRHIRenderer::ReleaseCachingResourcesFor(const ILayoutCache* Cacher)
{
	struct FReleaseCachingResourcesForContext
	{
		FSlateRHIRenderingPolicy* RenderPolicy;
		const ILayoutCache* Cacher;
	};
	FReleaseCachingResourcesForContext MarshalContext =
	{
		RenderingPolicy.Get(),
		Cacher,
	};
	ENQUEUE_RENDER_COMMAND(ReleaseCachingResourcesFor)(
		[MarshalContext](FRHICommandListImmediate& RHICmdList)
		{
			MarshalContext.RenderPolicy->ReleaseCachingResourcesFor(RHICmdList, MarshalContext.Cacher);
		}
	);
}

FSlateEndDrawingWindowsCommand::FSlateEndDrawingWindowsCommand(FSlateRHIRenderingPolicy& InPolicy, FSlateDrawBuffer* InDrawBuffer)
	: Policy(InPolicy)
	, DrawBuffer(InDrawBuffer)
{}

void FSlateEndDrawingWindowsCommand::Execute(FRHICommandListBase& CmdList)
{
	for ( auto& ElementList : DrawBuffer->GetWindowElementLists() )
	{
		ElementList->PostDraw_ParallelThread();
	}

	DrawBuffer->Unlock();
	Policy.EndDrawingWindows();
}

void FSlateEndDrawingWindowsCommand::EndDrawingWindows(FRHICommandListImmediate& RHICmdList, FSlateDrawBuffer* DrawBuffer, FSlateRHIRenderingPolicy& Policy)
{
	if (!RHICmdList.Bypass())
	{
		new (RHICmdList.AllocCommand<FSlateEndDrawingWindowsCommand>()) FSlateEndDrawingWindowsCommand(Policy, DrawBuffer);
	}
	else
	{
		FSlateEndDrawingWindowsCommand Cmd(Policy, DrawBuffer);
		Cmd.Execute(RHICmdList);
	}
}
