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

#include "CoreMinimal.h"
#include "IHeadMountedDisplay.h"
#include "IGoogleVRHMDPlugin.h"

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
#include "TickableObjectRenderThread.h"
#include "gvr.h"

class IRendererModule;
class UTexture2D;
class FGoogleVRHMD;
class FGoogleVRHMDCustomPresent;

class FGoogleVRSplash : public TSharedFromThis<FGoogleVRSplash>
{
public:

	FGoogleVRSplash(FGoogleVRHMD* InGVRHMD);
	~FGoogleVRSplash();

	void Init();
	void Show();
	void Hide();
	bool IsShown() { return bIsShown; }

	void Tick(float DeltaTime);
	bool IsTickable() const;
	void RenderStereoSplashScreen(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef DstTexture);
	void ForceRerenderSplashScreen();

private:

	class FGoogleVRSplashTicker : public FTickableObjectRenderThread, public TSharedFromThis<FGoogleVRSplashTicker>
	{
	public:
		FGoogleVRSplashTicker(FGoogleVRSplash* InSplash) : FTickableObjectRenderThread(false, true), pSplash(InSplash) {}

		virtual void Tick(float DeltaTime) override { pSplash->Tick(DeltaTime); }
		virtual TStatId GetStatId() const override  { RETURN_QUICK_DECLARE_CYCLE_STAT(FGoogleVRSplash, STATGROUP_Tickables); }
		virtual bool IsTickable() const override	{ return pSplash->IsTickable(); }
	protected:
		FGoogleVRSplash* pSplash;
	};

	void OnPreLoadMap(const FString&);
	void OnPostLoadMap(UWorld*);

	void AllocateSplashScreenRenderTarget();
	void SubmitBlackFrame();

	bool LoadDefaultSplashTexturePath();
	bool LoadTexture();
	void UnloadTexture();
	void UpdateSplashScreenEyeOffset();

public:
	bool bEnableSplashScreen;
	UTexture2D* SplashTexture;

	FString SplashTexturePath;
	FVector2D SplashTextureUVOffset;
	FVector2D SplashTextureUVSize;
	float RenderDistanceInMeter;
	float RenderScale;
	// View angle is used to reduce the async reprojection artifact
	// The splash screen will be hidden when the head rotated beyond half of the view angle
	// from its original orientation.
	float ViewAngleInDegree;

private:

	FGoogleVRHMD* GVRHMD;
	IRendererModule* RendererModule;
	FGoogleVRHMDCustomPresent* GVRCustomPresent;

	bool bInitialized;
	bool bIsShown;
	bool bSplashScreenRendered;
	FVector2D SplashScreenEyeOffset;
	TSharedPtr<FGoogleVRSplashTicker> RenderThreadTicker;

	gvr_mat4f SplashScreenRenderingHeadPose;
	FRotator SplashScreenRenderingOrientation;
};
#endif //GOOGLEVRHMD_SUPPORTED_PLATFORMS