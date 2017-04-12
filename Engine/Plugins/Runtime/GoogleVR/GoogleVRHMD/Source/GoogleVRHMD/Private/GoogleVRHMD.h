/* Copyright 2016 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "IGoogleVRHMDPlugin.h"
#include "HeadMountedDisplay.h"
#include "HeadMountedDisplayBase.h"
#include "SceneViewExtension.h"
#include "RHIStaticStates.h"
#include "SceneViewport.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "PostProcess/PostProcessHMD.h"
#include "GoogleVRHMDViewerPreviews.h"
#include "Classes/GoogleVRHMDFunctionLibrary.h"
#include "GoogleVRSplash.h"
#include "Containers/Queue.h"

DEFINE_LOG_CATEGORY_STATIC(LogHMD, Log, All);

#define LOG_VIEWER_DATA_FOR_GENERATION 0

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

#include "OpenGLDrvPrivate.h"
#include "OpenGLResources.h"

#include "gvr.h"

// GVR Api Reference
extern gvr_context* GVRAPI;

#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif

#include <GLES2/gl2.h>

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

#endif //GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS

#if GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS

#import <GVRSDK/GVROverlayView.h>

@class FOverlayViewDelegate;
@interface FOverlayViewDelegate : UIResponder<GVROverlayViewDelegate>
{
}
@end

#endif //GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS

// Forward decl
class FGoogleVRHMD;

class FGoogleVRHMDTexture2DSet : public FOpenGLTexture2D
{
public:

	FGoogleVRHMDTexture2DSet(
		class FOpenGLDynamicRHI* InGLRHI,
		GLuint InResource,
		GLenum InTarget,
		GLenum InAttachment,
		uint32 InSizeX,
		uint32 InSizeY,
		uint32 InSizeZ,
		uint32 InNumMips,
		uint32 InNumSamples,
		uint32 InNumSamplesTileMem,
		uint32 InArraySize,
		EPixelFormat InFormat,
		bool bInCubemap,
		bool bInAllocatedStorage,
		uint32 InFlags,
		uint8* InTextureRange
	);

	virtual ~FGoogleVRHMDTexture2DSet();

	static FGoogleVRHMDTexture2DSet* CreateTexture2DSet(
		FOpenGLDynamicRHI* InGLRHI,
		uint32 SizeX, uint32 SizeY,
		uint32 InNumSamples,
		uint32 InNumSamplesTileMem,
		EPixelFormat InFormat,
		uint32 InFlags);
};

class FGoogleVRHMDCustomPresent : public FRHICustomPresent
{
public:

	FGoogleVRHMDCustomPresent(FGoogleVRHMD* HMD);
	virtual ~FGoogleVRHMDCustomPresent();

    void Shutdown();

	gvr_frame* CurrentFrame;
	TRefCountPtr<FGoogleVRHMDTexture2DSet> TextureSet;
private:

	FGoogleVRHMD* HMD;

	bool bNeedResizeGVRRenderTarget;
	gvr_sizei RenderTargetSize;

	gvr_swap_chain* SwapChain;
	TQueue<gvr_mat4f> RenderingHeadPoseQueue;
	gvr_mat4f CurrentFrameRenderHeadPose;
	const gvr_buffer_viewport_list* CurrentFrameViewportList;
	bool bSkipPresent;

public:

	/**
	 * Allocates a render target texture.
	 *
	 * @param Index			(in) index of the buffer, changing from 0 to GetNumberOfBufferedFrames()
	 * @return				true, if texture was allocated; false, if the default texture allocation should be used.
	 */
	bool AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags);

	// Frame operations
	void UpdateRenderingViewportList(const gvr_buffer_viewport_list* BufferViewportList);
	void UpdateRenderingPose(gvr_mat4f InHeadPose);
	void UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI);

	void BeginRendering();
	void BeginRendering(const gvr_mat4f& RenderingHeadPose);
	void FinishRendering();

public:

	void CreateGVRSwapChain();
	///////////////////////////////////////
	// Begin FRHICustomPresent Interface //
	///////////////////////////////////////

	// Called when viewport is resized.
	virtual void OnBackBufferResize() override;

	// @param InOutSyncInterval - in out param, indicates if vsync is on (>0) or off (==0).
	// @return	true if normal Present should be performed; false otherwise. If it returns
	// true, then InOutSyncInterval could be modified to switch between VSync/NoVSync for the normal Present.
	virtual bool Present(int32& InOutSyncInterval) override;

	//// Called when rendering thread is acquired
	//virtual void OnAcquireThreadOwnership() {}

	//// Called when rendering thread is released
	//virtual void OnReleaseThreadOwnership() {}
};
#endif // GOOGLEVRHMD_SUPPORTED_PLATFORMS

/**
 * GoogleVR Head Mounted Display
 */
class FGoogleVRHMD : public FHeadMountedDisplayBase, public ISceneViewExtension, public FSelfRegisteringExec, public TSharedFromThis<FGoogleVRHMD, ESPMode::ThreadSafe>
{
	friend class FGoogleVRHMDCustomPresent;
	friend class FGoogleVRSplash;
public:

	FGoogleVRHMD();
	~FGoogleVRHMD();

	virtual FName GetDeviceName() const override
	{
		static FName DefaultName(TEXT("FGoogleVRHMD"));
		return DefaultName;
	}

	/** @return	True if the HMD was initialized OK */
	bool IsInitialized() const;

	/** Get current pose */
	void GetCurrentPose(FQuat& CurrentOrientation, FVector& CurrentPosition);

	/** Update viewportlist */
	void UpdateGVRViewportList() const;

	/** Update head pose, Should be called at the beginning of a frame**/
	void UpdateHeadPose();

	/** Helper method to get renderer module */
	IRendererModule* GetRendererModule();

	/** Refreshes the viewer if it might have changed */
	void RefreshViewerProfile();

	/** Get the Mobile Content rendering size set by Unreal. This value is affected by r.MobileContentScaleFactor.
	 *  On Android, this is also the size of the Surface View. When it is not set to native screen resolution,
	 *  the hardware scaler will be used.
	 */
	FIntPoint GetUnrealMobileContentSize();

	/** Get the RenderTarget size GoogleVRHMD is using for rendering the scene. */
	FIntPoint GetGVRHMDRenderTargetSize();

	/** Get the maximal effective render target size for the current windows size(surface size).
	 *  This value is got from gvr sdk. Which may change based on the viewer.
	 */
	FIntPoint GetGVRMaxRenderTargetSize();

	/** Set RenderTarget size to the default size and return the value. */
	FIntPoint SetRenderTargetSizeToDefault();

	/** Set the RenderTarget size with a scale factor
	 *  The scale factor will be multiplied with the MaxRenderTargetSize.
	 *  The range should be [0.1, 1.0]
	 */
	bool SetGVRHMDRenderTargetSize(float ScaleFactor, FIntPoint& OutRenderTargetSize);

	/** Set the RenderTargetSize with the desired value.
	 *  Note that the size will be rounded up to the next multiple of 4.
	 *  This is because Unreal need the rendertarget size is dividable by 4 for post process.
	 */
	bool SetGVRHMDRenderTargetSize(int DesiredWidth, int DesiredHeight, FIntPoint& OutRenderTargetSize);

	/** Enable/disable distortion correction */
	void SetDistortionCorrectionEnabled(bool bEnable);

	/** Change whether distortion correction is performed by GVR Api, or PostProcessHMD. Only supported on-device. */
	void SetDistortionCorrectionMethod(bool bUseGVRApiDistortionCorrection);

	/** Change the default viewer profile */
	bool SetDefaultViewerProfile(const FString& ViewerProfileURL);

	/** Generates a new distortion mesh of the given size */
	void SetDistortionMeshSize(EDistortionMeshSizeEnum MeshSize);

	/** Change the scaling factor used for applying the neck model offset */
	void SetNeckModelScale(float ScaleFactor);

	/** Check if distortion correction is enabled */
	bool GetDistortionCorrectionEnabled() const;

	/** Check which method distortion correction is using */
	bool IsUsingGVRApiDistortionCorrection() const;

	/** Get the scaling factor used for applying the neck model offset */
	float GetNeckModelScale() const;

	/** Check if application was launched in Vr." */
	bool IsVrLaunch() const;

	/** Check if the application is running in Daydream mode*/
	bool IsInDaydreamMode() const;

	void SetSPMEnable(bool bEnable) const;

	/**
	 * Returns the string representation of the data URI on which this activity's intent is operating.
	 * See Intent.getDataString() in the Android documentation.
	 */
	FString GetIntentData() const;

	/** Get the currently set viewer model */
	FString GetViewerModel();

	/** Get the currently set viewer vendor */
	FString GetViewerVendor();

private:

	/** Refresh RT so screen isn't black */
	void ApplicationResumeDelegate();

	/** Handle letting application know about GVR back event */
	void HandleGVRBackEvent();

	/** Helper method to generate index buffer for manual distortion rendering */
	void GenerateDistortionCorrectionIndexBuffer();

	/** Helper method to generate vertex buffer for manual distortion rendering */
	void GenerateDistortionCorrectionVertexBuffer(EStereoscopicPass Eye);

	/** Generates Distortion Correction Points*/
	void SetNumOfDistortionPoints(int32 XPoints, int32 YPoints);

	/** Get how many Unreal units correspond to one meter in the real world */
	float GetWorldToMetersScale() const;

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	/** Get the Viewport Rect from GVR */
	FIntRect CalculateGVRViewportRect(int RenderTargetSizeX, int RenderTargetSizeY, EStereoscopicPass StereoPassType);

	/** Get the Eye FOV from GVR SDK */
	gvr_rectf GetGVREyeFOV(int EyeIndex) const;
#endif

	/** Function get called when start loading a map*/
	void OnPreLoadMap(const FString&);

	/** Console command handlers */
	void DistortEnableCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void DistortMethodCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void RenderTargetSizeCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void NeckModelScaleCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	void DistortMeshSizeCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void ShowSplashCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void SplashScreenDistanceCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void SplashScreenRenderScaleCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void EnableSustainedPerformanceModeHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);

	/**
	Clutch to ensure that changes in r.ScreenPercentage are reflected in render target size.
	*/
	void CVarSinkHandler();
#endif
public:

	// public function for In editor distortion previews
	enum EViewerPreview
	{
		EVP_None				= 0,
		EVP_GoogleCardboard1	= 1,
		EVP_GoogleCardboard2	= 2,
		EVP_ViewMaster			= 3,
		EVP_SnailVR				= 4,
		EVP_RiTech2				= 5,
		EVP_Go4DC1Glass			= 6
	};

	/** Check which viewer is being used for previewing */
	static EViewerPreview GetPreviewViewerType();

	/** Get preview viewer interpupillary distance */
	static float GetPreviewViewerInterpupillaryDistance();

	/** Get preview viewer stereo projection matrix */
	static FMatrix GetPreviewViewerStereoProjectionMatrix(enum EStereoscopicPass StereoPass);

	/** Get preview viewer num vertices */
	static uint32 GetPreviewViewerNumVertices(enum EStereoscopicPass StereoPass);

	/** Get preview viewer vertices */
	static const FDistortionVertex* GetPreviewViewerVertices(enum EStereoscopicPass StereoPass);

public:
	// Public Components
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FGoogleVRHMDCustomPresent* CustomPresent;
	TSharedPtr<FGoogleVRSplash> GVRSplash;
#endif

private:
	// Updating Data
	bool		bStereoEnabled;
	bool		bHMDEnabled;
	bool		bDistortionCorrectionEnabled;
	bool		bUseGVRApiDistortionCorrection;
	bool		bUseOffscreenFramebuffers;
	bool		bIsInDaydreamMode;
	bool		bForceStopPresentScene;
	float		NeckModelScale;
	FQuat		CurHmdOrientation;
	FVector		CurHmdPosition;
	FRotator	DeltaControlRotation;    // same as DeltaControlOrientation but as rotator
	FQuat		DeltaControlOrientation; // same as DeltaControlRotation but as quat
	FQuat		BaseOrientation;

	// Drawing Data
	FIntPoint GVRRenderTargetSize;
	IRendererModule* RendererModule;
	uint16* DistortionMeshIndices;
	FDistortionVertex* DistortionMeshVerticesLeftEye;
	FDistortionVertex* DistortionMeshVerticesRightEye;

#if GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS
	GVROverlayView* OverlayView;
	FOverlayViewDelegate* OverlayViewDelegate;
#endif

	// Cached data that should only be updated once per frame
	mutable uint32		LastUpdatedCacheFrame;
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	mutable gvr_clock_time_point CachedFuturePoseTime;
	mutable gvr_mat4f CachedHeadPose;
	mutable FQuat CachedFinalHeadRotation;
	mutable FVector CachedFinalHeadPosition;
	mutable gvr_buffer_viewport_list* DistortedBufferViewportList;
	mutable gvr_buffer_viewport_list* NonDistortedBufferViewportList;
	mutable gvr_buffer_viewport_list* ActiveViewportList;
	mutable gvr_buffer_viewport* ScratchViewport;
#endif

	// Simulation data for previewing
	float PosePitch;
	float PoseYaw;

	// distortion mesh
	uint32 DistortionPointsX;
	uint32 DistortionPointsY;
	uint32 NumVerts;
	uint32 NumTris;
	uint32 NumIndices;

	/** Console commands */
	FAutoConsoleCommand DistortEnableCommand;
	FAutoConsoleCommand DistortMethodCommand;
	FAutoConsoleCommand RenderTargetSizeCommand;
	FAutoConsoleCommand NeckModelScaleCommand;

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FAutoConsoleCommand DistortMeshSizeCommand;
	FAutoConsoleCommand ShowSplashCommand;
	FAutoConsoleCommand SplashScreenDistanceCommand;
	FAutoConsoleCommand SplashScreenRenderScaleCommand;
	FAutoConsoleCommand EnableSustainedPerformanceModeCommand;

	FAutoConsoleVariableSink CVarSink;
#endif

public:

	//////////////////////////////////////////////////////
	// Begin ISceneViewExtension Pure-Virtual Interface //
	//////////////////////////////////////////////////////

	/**
	 * Called on game thread when creating the view family.
	 */
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	/**
	 * Called on game thread when creating the view.
	 */
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	/**
	 * Called on game thread when view family is about to be rendered.
	 */
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	/**
	 * Called on render thread at the start of rendering.
	 */
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
	/**
	 * Called on render thread at the start of rendering, for each view, after PreRenderViewFamily_RenderThread call.
	 */
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;

	///////////////////////////////////////////////////
	// Begin IStereoRendering Pure-Virtual Interface //
	///////////////////////////////////////////////////

	/**
	 * Whether or not stereo rendering is on this frame.
	 */
	virtual bool IsStereoEnabled() const override;

	/**
	 * Switches stereo rendering on / off. Returns current state of stereo.
	 * @return Returns current state of stereo (true / false).
	 */
	virtual bool EnableStereo(bool stereo = true) override;

	/**
	 * Adjusts the viewport rectangle for stereo, based on which eye pass is being rendered.
	 */
	virtual void AdjustViewRect(enum EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;

	/**
	 * Calculates the offset for the camera position, given the specified position, rotation, and world scale
	 */
	virtual void CalculateStereoViewOffset(const enum EStereoscopicPass StereoPassType, const FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation) override;

	/**
	 * Gets a projection matrix for the device, given the specified eye setup
	 */
	virtual FMatrix GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType, const float FOV) const override;

	/**
	 * Sets view-specific params (such as view projection matrix) for the canvas.
	 */
	virtual void InitCanvasFromView(class FSceneView* InView, class UCanvas* Canvas) override;

	//////////////////////////////////////////////
	// Begin IStereoRendering Virtual Interface //
	//////////////////////////////////////////////

	///**
	// * Whether or not stereo rendering is on on next frame. Useful to determine if some preparation work
	// * should be done before stereo got enabled in next frame.
	// */
	//virtual bool IsStereoEnabledOnNextFrame() const { return IsStereoEnabled(); }

	///**
	// * Gets the percentage bounds of the safe region to draw in.  This allows things like stat rendering to appear within the readable portion of the stereo view.
	// * @return	The centered percentage of the view that is safe to draw readable text in
	// */
	//virtual FVector2D GetTextSafeRegionBounds() const { return FVector2D(0.75f, 0.75f); }

	/**
	 * Returns eye render params, used from PostProcessHMD, RenderThread.
	 */
	virtual void GetEyeRenderParams_RenderThread(const struct FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const override;

	///**
	// * Returns timewarp matrices, used from PostProcessHMD, RenderThread.
	// */
	//virtual void GetTimewarpMatrices_RenderThread(const struct FRenderingCompositePassContext& Context, FMatrix& EyeRotationStart, FMatrix& EyeRotationEnd) const {}

	// Optional methods to support rendering into a texture.
	/**
	 * Updates viewport for direct rendering of distortion. Should be called on a game thread.
	 * Optional SViewport* parameter can be used to access SWindow object.
	 */
	virtual void UpdateViewport(bool bUseSeparateRenderTarget, const class FViewport& Viewport, class SViewport* = nullptr) override;

	/**
	 * Calculates dimensions of the render target texture for direct rendering of distortion.
	 */
	virtual void CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY) override;

	/**
	 * Returns true, if render target texture must be re-calculated.
	 */
	virtual bool NeedReAllocateViewportRenderTarget(const class FViewport& Viewport) override;

	// Whether separate render target should be used or not.
	virtual bool ShouldUseSeparateRenderTarget() const override;

	// Renders texture into a backbuffer. Could be empty if no rendertarget texture is used, or if direct-rendering
	// through RHI bridge is implemented.
	virtual void RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture) const override;

	///**
	// * Called after Present is called.
	// */
	//virtual void FinishRenderingFrame_RenderThread(class FRHICommandListImmediate& RHICmdList) {}

	///**
	// * Returns orthographic projection , used from Canvas::DrawItem.
	// */
	//virtual void GetOrthoProjection(int32 RTWidth, int32 RTHeight, float OrthoDistance, FMatrix OrthoProjection[2]) const
	//{
	//	OrthoProjection[0] = OrthoProjection[1] = FMatrix::Identity;
	//	OrthoProjection[1] = FTranslationMatrix(FVector(OrthoProjection[1].M[0][3] * RTWidth * .25 + RTWidth * .5, 0, 0));
	//}

	///**
	// * Sets screen percentage to be used for stereo rendering.
	// *
	// * @param ScreenPercentage	(in) Specifies the screen percentage to be used in VR mode. Use 0.0f value to reset to default value.
	// */
	//virtual void SetScreenPercentage(float InScreenPercentage) {}
	//
	///**
	// * Returns screen percentage to be used for stereo rendering.
	// *
	// * @return (float)	The screen percentage to be used in stereo mode. 0.0f, if default value is used.
	// */
	//virtual float GetScreenPercentage() const { return 0.0f; }

	/**
	 * Sets near and far clipping planes (NCP and FCP) for stereo rendering. Similar to 'stereo ncp= fcp' console command, but NCP and FCP set by this
	 * call won't be saved in .ini file.
	 *
	 * @param NCP				(in) Near clipping plane, in centimeters
	 * @param FCP				(in) Far clipping plane, in centimeters
	 */
	virtual void SetClippingPlanes(float NCP, float FCP) override;

	/**
	 * Returns currently active custom present.
	 */
	virtual FRHICustomPresent* GetCustomPresent() override;

	/**
	 * Returns number of required buffered frames.
	 */
	virtual uint32 GetNumberOfBufferedFrames() const override;

	/**
	 * Allocates a render target texture.
	 *
	 * @param Index			(in) index of the buffer, changing from 0 to GetNumberOfBufferedFrames()
	 * @return				true, if texture was allocated; false, if the default texture allocation should be used.
	 */
	virtual bool AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) override;

	//////////////////////////////////////////////////////
	// Begin IHeadMountedDisplay Pure-Virtual Interface //
	//////////////////////////////////////////////////////

	/**
	 * Returns true if HMD is currently connected.
	 */
	virtual bool IsHMDConnected() override;

	/**
	 * Whether or not switching to stereo is enabled; if it is false, then EnableStereo(true) will do nothing.
	 */
	virtual bool IsHMDEnabled() const override;

	/**
	 * Enables or disables switching to stereo.
	 */
	virtual void EnableHMD(bool bEnable = true) override;

	/**
	 * Returns the family of HMD device implemented
	 */
	virtual EHMDDeviceType::Type GetHMDDeviceType() const override;

    /**
     * Get the name or id of the display to output for this HMD.
     */
	virtual bool	GetHMDMonitorInfo(MonitorInfo&) override;

    /**
	 * Calculates the FOV, based on the screen dimensions of the device. Original FOV is passed as params.
	 */
	virtual void	GetFieldOfView(float& InOutHFOVInDegrees, float& InOutVFOVInDegrees) const override;

	/**
	 * Whether or not the HMD supports positional tracking (either via camera or other means)
	 */
	virtual bool	DoesSupportPositionalTracking() const override;

	/**
	 * If the device has positional tracking, whether or not we currently have valid tracking
	 */
	virtual bool	HasValidTrackingPosition() override;

	/**
	 * If the HMD supports positional tracking via a camera, this returns the frustum properties (all in game-world space) of the tracking camera.
	 */
	virtual void	GetPositionalTrackingCameraProperties(FVector& OutOrigin, FQuat& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const override;

	/**
	 * Accessors to modify the interpupillary distance (meters)
	 */
	virtual void	SetInterpupillaryDistance(float NewInterpupillaryDistance) override;
	virtual float	GetInterpupillaryDistance() const override;

    /**
     * Get the current orientation and position reported by the HMD.
     */
    virtual void GetCurrentOrientationAndPosition(FQuat& CurrentOrientation, FVector& CurrentPosition) override;

	/**
	 * Rebase the input position and orientation to that of the HMD's base
	 */
	virtual void RebaseObjectOrientationAndPosition(FVector& Position, FQuat& Orientation) const override;

	/**
	 * Get the ISceneViewExtension for this HMD, or none.
	 */
	virtual TSharedPtr<class ISceneViewExtension, ESPMode::ThreadSafe> GetViewExtension() override;

	/**
     * Apply the orientation of the headset to the PC's rotation.
     * If this is not done then the PC will face differently than the camera,
     * which might be good (depending on the game).
     */
	virtual void ApplyHmdRotation(class APlayerController* PC, FRotator& ViewRotation) override;

	/**
	 * Apply the orientation and position of the headset to the Camera.
	 */
	virtual bool UpdatePlayerCamera(FQuat& CurrentOrientation, FVector& CurrentPosition) override;

	/**
	 * Returns 'false' if chromatic aberration correction is off.
	 */
	virtual bool IsChromaAbCorrectionEnabled() const override;

	/**
	 * Exec handler to allow console commands to be passed through to the HMD for debugging
	 */
    virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;

	/** Returns true if positional tracking enabled and working. */
	virtual bool IsPositionalTrackingEnabled() const override;

	/**
	 * Returns true, if head tracking is allowed. Most common case: it returns true when GEngine->IsStereoscopic3D() is true,
	 * but some overrides are possible.
	 */
	virtual bool IsHeadTrackingAllowed() const override;

	/**
	 * Resets orientation by setting roll and pitch to 0, assuming that current yaw is forward direction and assuming
	 * current position as a 'zero-point' (for positional tracking).
	 *
	 * @param Yaw				(in) the desired yaw to be set after orientation reset.
	 */
	virtual void ResetOrientationAndPosition(float Yaw = 0.f) override;

	/////////////////////////////////////////////////
	// Begin IHeadMountedDisplay Virtual Interface //
	/////////////////////////////////////////////////

 // 	/**
	// * Gets the scaling factor, applied to the post process warping effect
	// */
	//virtual float GetDistortionScalingFactor() const { return 0; }

	///**
	// * Gets the offset (in clip coordinates) from the center of the screen for the lens position
	// */
	//virtual float GetLensCenterOffset() const { return 0; }

	///**
	// * Gets the barrel distortion shader warp values for the device
	// */
	//virtual void GetDistortionWarpValues(FVector4& K) const  { }

	///**
	// * Gets the chromatic aberration correction shader values for the device.
	// * Returns 'false' if chromatic aberration correction is off.
	// */
	//virtual bool GetChromaAbCorrectionValues(FVector4& K) const  { return false; }

	///**
	// * Saves / loads pre-fullscreen rectangle. Could be used to store saved original window position
	// * before switching to fullscreen mode.
	// */
	//virtual void PushPreFullScreenRect(const FSlateRect& InPreFullScreenRect);
	//virtual void PopPreFullScreenRect(FSlateRect& OutPreFullScreenRect);

	/**
	 * Resets orientation by setting roll and pitch to 0, assuming that current yaw is forward direction. Position is not changed.
	 *
	 * @param Yaw				(in) the desired yaw to be set after orientation reset.
	 */
	virtual void ResetOrientation(float Yaw = 0.f) override;

	/**
	 * Resets position, assuming current position as a 'zero-point'.
	 */
	virtual void ResetPosition() override;

	/**
	 * Sets base orientation by setting yaw, pitch, roll, assuming that this is forward direction.
	 * Position is not changed.
	 *
	 * @param BaseRot			(in) the desired orientation to be treated as a base orientation.
	 */
	virtual void SetBaseRotation(const FRotator& BaseRot) override;

	/**
	 * Returns current base orientation of HMD as yaw-pitch-roll combination.
	 */
	virtual FRotator GetBaseRotation() const override;

	/**
	 * Sets base orientation, assuming that this is forward direction.
	 * Position is not changed.
	 *
	 * @param BaseOrient		(in) the desired orientation to be treated as a base orientation.
	 */
	virtual void SetBaseOrientation(const FQuat& BaseOrient) override;

	/**
	 * Returns current base orientation of HMD as a quaternion.
	 */
	virtual FQuat GetBaseOrientation() const override;

	/**
	* @return true if a hidden area mesh is available for the device.
	*/
	virtual bool HasHiddenAreaMesh() const override;

	/**
	* @return true if a visible area mesh is available for the device.
	*/
	virtual bool HasVisibleAreaMesh() const override;

	/**
	* Optional method to draw a view's hidden area mesh where supported.
	* This can be used to avoid rendering pixels which are not included as input into the final distortion pass.
	*/
	virtual void DrawHiddenAreaMesh_RenderThread(class FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const override;

	/**
	* Optional method to draw a view's visible area mesh where supported.
	* This can be used instead of a full screen quad to avoid rendering pixels which are not included as input into the final distortion pass.
	*/
	virtual void DrawVisibleAreaMesh_RenderThread(class FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const override;

	virtual void DrawDistortionMesh_RenderThread(struct FRenderingCompositePassContext& Context, const FIntPoint& TextureSize) override;

	///**
	// * This method is able to change screen settings right before any drawing occurs.
	// * It is called at the beginning of UGameViewportClient::Draw() method.
	// * We might remove this one as UpdatePostProcessSettings should be able to capture all needed cases
	// */
	//virtual void UpdateScreenSettings(const FViewport* InViewport) {}

	///**
	// * Allows to override the PostProcessSettings in the last moment e.g. allows up sampled 3D rendering
	// */
	//virtual void UpdatePostProcessSettings(FPostProcessSettings*) {}

	///**
	// * Draw desired debug information related to the HMD system.
	// * @param Canvas The canvas on which to draw.
	// */
	//virtual void DrawDebug(UCanvas* Canvas) {}

	///**
	// * Passing key events to HMD.
	// * If returns 'false' then key will be handled by PlayerController;
	// * otherwise, key won't be handled by the PlayerController.
	// */
	virtual bool HandleInputKey(class UPlayerInput*, const struct FKey& Key, enum EInputEvent EventType, float AmountDepressed, bool bGamepad) override;

	/**
	 * Passing touch events to HMD.
	 * If returns 'false' then touch will be handled by PlayerController;
	 * otherwise, touch won't be handled by the PlayerController.
	 */
	virtual bool HandleInputTouch(uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, FDateTime DeviceTimestamp, uint32 TouchpadIndex) override;

	///**
	// * This method is called when playing begins. Useful to reset all runtime values stored in the plugin.
	// */
	//virtual void OnBeginPlay() {}

	///**
	// * This method is called when playing ends. Useful to reset all runtime values stored in the plugin.
	// */
	//virtual void OnEndPlay() {}

	/**
	 * This method is called when new game frame begins (called on a game thread).
	 */
	virtual bool OnStartGameFrame( FWorldContext& WorldContext ) override;

	/**
	 * This method is called when game frame ends (called on a game thread).
	 */
	virtual bool OnEndGameFrame( FWorldContext& WorldContext ) override;

	///**
	// * Additional optional distorion rendering parameters
	// * @todo:  Once we can move shaders into plugins, remove these!
	// */
	//virtual FTexture* GetDistortionTextureLeft() const {return NULL;}
	//virtual FTexture* GetDistortionTextureRight() const {return NULL;}
	//virtual FVector2D GetTextureOffsetLeft() const {return FVector2D::ZeroVector;}
	//virtual FVector2D GetTextureOffsetRight() const {return FVector2D::ZeroVector;}
	//virtual FVector2D GetTextureScaleLeft() const {return FVector2D::ZeroVector;}
	//virtual FVector2D GetTextureScaleRight() const {return FVector2D::ZeroVector;}

	//virtual bool NeedsUpscalePostProcessPass()  { return false; }


	/**
	 * Returns version string.
	 */
	virtual FString GetVersionString() const override;

};
