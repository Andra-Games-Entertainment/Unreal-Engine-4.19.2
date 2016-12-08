// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/ThreadSafeBool.h"
#include "TickableObjectRenderThread.h"

// Base class for asynchronous loading splash.
class FAsyncLoadingSplash : public TSharedFromThis<FAsyncLoadingSplash>, public FGCObject
{
protected:
	class FTicker : public FTickableObjectRenderThread, public TSharedFromThis<FTicker>
	{
	public:
		FTicker(FAsyncLoadingSplash* InSplash) : FTickableObjectRenderThread(false, true), pSplash(InSplash) {}

		virtual void Tick(float DeltaTime) override { pSplash->Tick(DeltaTime); }
		virtual TStatId GetStatId() const override  { RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncLoadingSplash, STATGROUP_Tickables); }
		virtual bool IsTickable() const override	{ return pSplash->IsTickable(); }
	protected:
		FAsyncLoadingSplash* pSplash;
	};

public:
	struct FSplashDesc
	{
		UTexture2D*			LoadingTexture;					// a UTexture pointer, either loaded manually or passed externally.
		FString				TexturePath;					// a path to a texture for auto loading, can be empty if LoadingTexture is specified explicitly
		FTransform			TransformInMeters;				// transform of center of quad (meters)
		FVector2D			QuadSizeInMeters;				// dimensions in meters
		FQuat				DeltaRotation;					// a delta rotation that will be added each rendering frame (half rate of full vsync)
		FTextureRHIRef		LoadedTexture;					// texture reference for when a TexturePath or UTexture is not available
		FVector2D			TextureOffset;					// texture offset amount from the top left corner
		FVector2D			TextureScale;					// texture scale
		bool				bNoAlphaChannel;				// whether the splash layer uses it's alpha channel

		FSplashDesc() : LoadingTexture(nullptr)
			, TransformInMeters(FVector(4.0f, 0.f, 0.f))
			, QuadSizeInMeters(3.f, 3.f)
			, DeltaRotation(FQuat::Identity)
			, LoadedTexture(nullptr)
			, TextureOffset(0.0f, 0.0f)
			, TextureScale(1.0f, 1.0f)
			, bNoAlphaChannel(false)
		{
		}
		bool operator==(const FSplashDesc& d) const
		{
			return LoadingTexture == d.LoadingTexture && TexturePath == d.TexturePath && LoadedTexture == d.LoadedTexture &&
				TransformInMeters.Equals(d.TransformInMeters) && 
				QuadSizeInMeters == d.QuadSizeInMeters && DeltaRotation.Equals(d.DeltaRotation) &&
				TextureOffset == d.TextureOffset && TextureScale == d.TextureScale;
		}
	};

	FAsyncLoadingSplash();
	virtual ~FAsyncLoadingSplash();

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// End of FGCObject interface

	// FTickableObjectRenderThread implementation
	virtual void Tick(float DeltaTime) {}
	virtual bool IsTickable() const { return IsLoadingStarted() && !IsDone(); }
	// End of FTickableObjectRenderThread interface

	virtual void Startup();
	virtual void Shutdown();

	virtual bool IsLoadingStarted() const	{ return LoadingStarted; }
	virtual bool IsDone() const				{ return LoadingCompleted; }

	virtual void OnLoadingBegins();
	virtual void OnLoadingEnds();

	virtual bool AddSplash(const FSplashDesc&);
	virtual void ClearSplashes();
	virtual bool GetSplash(unsigned index, FSplashDesc& OutDesc);

	virtual void SetAutoShow(bool bInAuto) { bAutoShow = bInAuto; }
	virtual bool IsAutoShow() const { return bAutoShow; }

	virtual void SetLoadingIconMode(bool bInLoadingIconMode) { LoadingIconMode = bInLoadingIconMode; }
	virtual bool IsLoadingIconMode() const { return LoadingIconMode; }

	enum EShowType
	{
		None,
		ShowAtLoading,
		ShowManually
	};

	virtual void Show(enum EShowType) = 0;
	virtual void Hide(enum EShowType) = 0;

	// delegate method, called when loading begins
	void OnPreLoadMap(const FString&) { OnLoadingBegins(); }

	// delegate method, called when loading ends
	void OnPostLoadMap() { OnLoadingEnds(); }

protected:
	void LoadTexture(FSplashDesc& InSplashDesc);
	void UnloadTexture(FSplashDesc& InSplashDesc);

	virtual uint32 GetTotalNumberOfLayersSupported() const = 0;

	TSharedPtr<FTicker>	RenTicker;

	mutable FCriticalSection SplashScreensLock;
	UPROPERTY()
	TArray<FSplashDesc> SplashScreenDescs;

	FThreadSafeBool		LoadingCompleted;
	FThreadSafeBool		LoadingStarted;
	FThreadSafeBool		LoadingIconMode;		// this splash screen is a simple loading icon (if supported)
	bool				bAutoShow : 1;			// whether or not show splash screen automatically (when LoadMap is called)
	bool				bInitialized : 1;
};
