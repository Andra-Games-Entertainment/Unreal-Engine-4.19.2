// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VREditorMode.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/ConstructorHelpers.h"
#include "SDockTab.h"
#include "Engine/EngineTypes.h"
#include "Components/SceneComponent.h"
#include "Misc/ConfigCacheIni.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/WorldSettings.h"
#include "DrawDebugHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "VREditorUISystem.h"
#include "VIBaseTransformGizmo.h"
#include "ViewportWorldInteraction.h"
#include "VREditorPlacement.h"
#include "VREditorAvatarActor.h"
#include "VREditorTeleporter.h"
#include "VREditorAutoScaler.h"
#include "VREditorStyle.h"
#include "VREditorAssetContainer.h"
#include "Framework/Notifications/NotificationManager.h"
#include "CameraController.h"
#include "EngineGlobals.h"
#include "ILevelEditor.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "SLevelViewport.h"
#include "MotionControllerComponent.h"
#include "EngineAnalytics.h"
#include "IHeadMountedDisplay.h"
#include "Interfaces/IAnalyticsProvider.h"

#include "IViewportInteractionModule.h"
#include "VREditorMotionControllerInteractor.h"

#include "ViewportWorldInteractionManager.h"
#include "EditorWorldExtension.h"
#include "SequencerSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "VREditorActions.h"
#include "EditorModes.h"
#include "VRModeSettings.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"


#define LOCTEXT_NAMESPACE "VREditorMode"

namespace VREd
{
	static FAutoConsoleVariable DefaultVRNearClipPlane(TEXT("VREd.DefaultVRNearClipPlane"), 5.0f, TEXT("The near clip plane to use for VR"));
	static FAutoConsoleVariable SlateDragDistanceOverride( TEXT( "VREd.SlateDragDistanceOverride" ), 40.0f, TEXT( "How many pixels you need to drag before a drag and drop operation starts in VR" ) );
	static FAutoConsoleVariable DefaultWorldToMeters(TEXT("VREd.DefaultWorldToMeters"), 100.0f, TEXT("Default world to meters scale"));

	static FAutoConsoleVariable ShowHeadVelocity( TEXT( "VREd.ShowHeadVelocity" ), 0, TEXT( "Whether to draw a debug indicator that shows how much the head is accelerating" ) );
	static FAutoConsoleVariable HeadVelocitySmoothing( TEXT( "VREd.HeadVelocitySmoothing" ), 0.95f, TEXT( "How much to smooth out head velocity data" ) );
	static FAutoConsoleVariable HeadVelocityMinRadius( TEXT( "VREd.HeadVelocityMinRadius" ), 0.0f, TEXT( "How big the inner circle of the head velocity ring should be" ) );
	static FAutoConsoleVariable HeadVelocityMaxRadius( TEXT( "VREd.HeadVelocityMaxRadius" ), 10.0f, TEXT( "How big the outer circle of the head velocity ring should be" ) );
	static FAutoConsoleVariable HeadVelocityMinLineThickness( TEXT( "VREd.HeadVelocityMinLineThickness" ), 0.05f, TEXT( "How thick the head velocity ring lines should be" ) );
	static FAutoConsoleVariable HeadVelocityMaxLineThickness( TEXT( "VREd.HeadVelocityMaxLineThickness" ), 0.4f, TEXT( "How thick the head velocity ring lines should be" ) );
	static FAutoConsoleVariable HeadLocationMaxVelocity( TEXT( "VREd.HeadLocationMaxVelocity" ), 25.0f, TEXT( "For head velocity indicator, the maximum location velocity in cm/s" ) );
	static FAutoConsoleVariable HeadRotationMaxVelocity( TEXT( "VREd.HeadRotationMaxVelocity" ), 80.0f, TEXT( "For head velocity indicator, the maximum rotation velocity in degrees/s" ) );
	static FAutoConsoleVariable HeadLocationVelocityOffset( TEXT( "VREd.HeadLocationVelocityOffset" ), TEXT( "X=20, Y=0, Z=5" ), TEXT( "Offset relative to head for location velocity debug indicator" ) );
	static FAutoConsoleVariable HeadRotationVelocityOffset( TEXT( "VREd.HeadRotationVelocityOffset" ), TEXT( "X=20, Y=0, Z=-5" ), TEXT( "Offset relative to head for rotation velocity debug indicator" ) );
	static FAutoConsoleVariable SFXMultiplier(TEXT("VREd.SFXMultiplier"), 6.0f, TEXT("Default Sound Effect Volume Multiplier"));

	static FAutoConsoleVariable AllowVRModeFading(TEXT("VREd.AllowVRModeFading"), 1, TEXT("Whether to allow the VR Mode view to fade "));
}

const FString UVREditorMode::AssetContainerPath = FString("/Engine/VREditor/VREditorAssetContainerData");

UVREditorMode::UVREditorMode() :
	Super(),
	bWantsToExitMode( false ),
	bIsFullyInitialized( false ),
	AppTimeModeEntered( FTimespan::Zero() ),
	AvatarActor( nullptr ),
	CalibrationTransformOffset( FTransform::Identity ),
	RoomSpacePivotActor( nullptr ),
	CameraBaseActor( nullptr ),
	HMDTransformActor( nullptr ),
	FlashlightComponent( nullptr ),
	bIsFlashlightOn( false ),
	MotionControllerID( 0 ),	// @todo vreditor minor: We only support a single controller, and we assume the first controller are the motion controls
	UISystem( nullptr ),
	TeleportActor( nullptr ),
	AutoScalerSystem( nullptr ),
	WorldInteraction( nullptr ),
	LeftHandInteractor( nullptr ),
	RightHandInteractor( nullptr ),
	bFirstTick( true ),
	SavedWorldToMetersScaleForPIE( 100.f ),
	bStartedPlayFromVREditor( false ),
	bStartedPlayFromVREditorSimulate( false ),
	AssetContainer( nullptr )
{
}

void UVREditorMode::DoToggleCalibrationMode()
{
	this->SetIsCalibrating( !bIsCalibrating );
}

void UVREditorMode::Init()
{
	// @todo vreditor urgent: Turn on global editor hacks for VR Editor mode
	GEnableVREditorHacks = true;

	bIsFullyInitialized = false;
	bWantsToExitMode = false;

	AppTimeModeEntered = FTimespan::FromSeconds( FApp::GetCurrentTime() );

	// Take note of VREditor activation
	if( FEngineAnalytics::IsAvailable() )
	{
		FEngineAnalytics::GetProvider().RecordEvent( TEXT( "Editor.Usage.InitVREditorMode" ) );
	}

	// Setting up colors
	Colors.SetNumZeroed( (int32)EColors::TotalCount );
	{	
		Colors[ (int32)EColors::DefaultColor ] = FLinearColor(0.075f, 0.084f, 0.701f, 1.0f);
		Colors[ (int32)EColors::SelectionColor ] = FLinearColor(1.0f, 0.467f, 0.0f, 1.f);
		Colors[ (int32)EColors::WorldDraggingColor ] = FLinearColor(0.106, 0.487, 0.106, 1.0f);
		Colors[ (int32)EColors::UIColor ] = FLinearColor(0.22f, 0.7f, 0.98f, 1.0f);
		Colors[ (int32)EColors::UISelectionBarColor ] = FLinearColor( 0.025f, 0.025f, 0.025f, 1.0f );
		Colors[ (int32)EColors::UISelectionBarHoverColor ] = FLinearColor( 0.1f, 0.1f, 0.1f, 1.0f );
		Colors[ (int32)EColors::UICloseButtonColor ] = FLinearColor( 0.1f, 0.1f, 0.1f, 1.0f );
		Colors[ (int32)EColors::UICloseButtonHoverColor ] = FLinearColor( 1.0f, 1.0f, 1.0f, 1.0f );
	}

	{
		UEditorWorldExtensionCollection* Collection = GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions(GetWorld());
		check(Collection != nullptr);
		WorldInteraction = Cast<UViewportWorldInteraction>( Collection->FindExtension( UViewportWorldInteraction::StaticClass() ) );
		check( WorldInteraction != nullptr );
	}

	// Setup the asset container.
	AssetContainer = LoadObject<UVREditorAssetContainer>(nullptr, *UVREditorMode::AssetContainerPath);
	check(AssetContainer != nullptr);

	bIsFullyInitialized = true;
}

/*
* @EventName Editor.Usage.EnterVRMode
*
* @Trigger Entering VR editing mode
*
* @Type Static
*
* @EventParam HMDDevice (string) The name of the HMD Device type
*
* @Source Editor
*
* @Owner Lauren.Ridge
*
*/
void UVREditorMode::Shutdown()
{
	bIsFullyInitialized = false;
	
	AvatarActor = nullptr;
	FlashlightComponent = nullptr;
	UISystem = nullptr;
	TeleportActor = nullptr;
	AutoScalerSystem = nullptr;
	WorldInteraction = nullptr;
	LeftHandInteractor = nullptr;
	RightHandInteractor = nullptr;
	AssetContainer = nullptr;

	// @todo vreditor urgent: Disable global editor hacks for VR Editor mode
	GEnableVREditorHacks = false;

	FEditorDelegates::EndPIE.RemoveAll(this);
}

void UVREditorMode::Enter()
{
	bWantsToExitMode = false;

	{
		WorldInteraction->OnPreWorldInteractionTick().AddUObject( this, &UVREditorMode::PreTick );
		WorldInteraction->OnPostWorldInteractionTick().AddUObject( this, &UVREditorMode::PostTick );
	}

	FEditorDelegates::PostPIEStarted.AddUObject( this, &UVREditorMode::PostPIEStarted );
	FEditorDelegates::PrePIEEnded.AddUObject( this, &UVREditorMode::PrePIEEnded );
	FEditorDelegates::EndPIE.AddUObject(this, &UVREditorMode::OnEndPIE);
	FEditorDelegates::OnPreSwitchBeginPIEAndSIE.AddUObject(this, &UVREditorMode::OnPreSwitchPIEAndSIE);
	FEditorDelegates::OnSwitchBeginPIEAndSIE.AddUObject(this, &UVREditorMode::OnSwitchPIEAndSIE);

	CalibrateConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT( "VREd.Calibrate" ),
		*LOCTEXT( "CommandText_Calibrate", "Calibrate" ).ToString(),
		FConsoleCommandDelegate::CreateUObject( this, &UVREditorMode::DoToggleCalibrationMode ) );

	TeleportToTaggedActorCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT( "VREd.TeleportToTaggedActor" ),
		*LOCTEXT( "CommandText_TeleportToTaggedActor", "TeleportToTaggedActor" ).ToString(),
		FConsoleCommandWithArgsDelegate::CreateUObject( this, &UVREditorMode::TeleportToTaggedActor ) );

	// @todo vreditor: We need to make sure the user can never switch to orthographic mode, or activate settings that
	// would disrupt the user's ability to view the VR scene.

	// @todo vreditor: Don't bother drawing toolbars in VR, or other things that won't matter in VR

	{
		const TSharedRef< ILevelEditor >& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor").GetFirstLevelEditor().ToSharedRef();

		// Do we have an active perspective viewport that is valid for VR?  If so, go ahead and use that.
		TSharedPtr<SLevelViewport> ExistingActiveLevelViewport;
		{
			TSharedPtr<ILevelViewport> ActiveLevelViewport = LevelEditor->GetActiveViewportInterface();
			if(ActiveLevelViewport.IsValid())
			{
				ExistingActiveLevelViewport = StaticCastSharedRef< SLevelViewport >(ActiveLevelViewport->AsWidget());
			}
		}

		StartViewport(ExistingActiveLevelViewport);

		if( bActuallyUsingVR )
		{
			// Tell Slate to require a larger pixel distance threshold before the drag starts.  This is important for things 
			// like Content Browser drag and drop.
			SavedEditorState.DragTriggerDistance = FSlateApplication::Get().GetDragTriggerDistance();
			FSlateApplication::Get().SetDragTriggerDistance( VREd::SlateDragDistanceOverride->GetFloat() );

			// When actually in VR, make sure the transform gizmo is big!
			SavedEditorState.TransformGizmoScale = WorldInteraction->GetTransformGizmoScale();
			WorldInteraction->SetTransformGizmoScale(GetDefault<UVRModeSettings>()->GizmoScale);
			WorldInteraction->SetShouldSuppressExistingCursor(true);
			WorldInteraction->SetInVR(true);

			// Take note of VREditor entering (only if actually in VR)
			if (FEngineAnalytics::IsAvailable())
			{
				TArray< FAnalyticsEventAttribute > Attributes;
				FString HMDName = GEditor->HMDDevice->GetDeviceName().ToString();
				Attributes.Add(FAnalyticsEventAttribute(TEXT("HMDDevice"), HMDName));
				FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.EnterVRMode"), Attributes);
			}
		}
	}

	// Switch us back to placement mode and close any open sequencer windows
	FVREditorActionCallbacks::ChangeEditorModes(FBuiltinEditorModes::EM_Placement);
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	LevelEditorModule.GetLevelEditorTabManager()->InvokeTab(FTabId("Sequencer"))->RequestCloseTab();

	// Setup our avatar
	if (AvatarActor == nullptr)
	{
		const bool bWithSceneComponent = true;
		AvatarActor = SpawnTransientSceneActor<AVREditorAvatarActor>(TEXT("AvatarActor"), bWithSceneComponent);
		AvatarActor->Init(this);

		WorldInteraction->AddActorToExcludeFromHitTests( AvatarActor );	
	}

	// @TODO VREDITOR DEMO HACKS
	{
		if( RoomSpacePivotActor == nullptr )
		{
			RoomSpacePivotActor = SpawnTransientSceneActor<AVRReplicatedActor>( TEXT( "RoomSpacePivot" ) );
			UStaticMesh* Mesh = LoadObject<UStaticMesh>( nullptr, TEXT("/Engine/EditorMeshes/Axis_Guide") );
			check( Mesh != nullptr );
			RoomSpacePivotActor->GetStaticMeshComponent()->SetStaticMesh( Mesh );
			RoomSpacePivotActor->GetStaticMeshComponent()->SetCollisionEnabled( ECollisionEnabled::NoCollision );
			RoomSpacePivotActor->GetStaticMeshComponent()->SetCollisionResponseToAllChannels( ECollisionResponse::ECR_Ignore );
			WorldInteraction->AddActorToExcludeFromHitTests( RoomSpacePivotActor );

			RoomSpacePivotActor->GetRootComponent()->SetVisibility( bIsCalibrating );
		}
		if( CameraBaseActor == nullptr )
		{
			CameraBaseActor = SpawnTransientSceneActor<ACameraBaseActor>( TEXT( "CameraBase" ) );
			UStaticMesh* Mesh = LoadObject<UStaticMesh>( nullptr, TEXT( "/Engine/VREditor/ncamCalibPlane" ) );
			check( Mesh != nullptr );
			CameraBaseActor->GetStaticMeshComponent()->SetStaticMesh( Mesh );
			CameraBaseActor->GetStaticMeshComponent()->SetCollisionEnabled( ECollisionEnabled::NoCollision );
			CameraBaseActor->GetStaticMeshComponent()->SetCollisionResponseToAllChannels( ECollisionResponse::ECR_Ignore );
			WorldInteraction->AddActorToExcludeFromHitTests( CameraBaseActor );

			CameraBaseActor->GetRootComponent()->SetVisibility( bIsCalibrating );
		}
		if (HMDTransformActor == nullptr)
		{
			HMDTransformActor = SpawnTransientSceneActor<AHMDTransformActor>( TEXT( "HMDTransform" ) );
			UStaticMesh* Mesh = LoadObject<UStaticMesh>( nullptr, TEXT( "/Engine/VREditor/Devices/Generic/GenericHMD" ) );
			check( Mesh != nullptr );
			HMDTransformActor->GetStaticMeshComponent()->SetStaticMesh( Mesh );
			HMDTransformActor->GetStaticMeshComponent()->SetCollisionEnabled( ECollisionEnabled::NoCollision );
			HMDTransformActor->GetStaticMeshComponent()->SetCollisionResponseToAllChannels( ECollisionResponse::ECR_Ignore );
			WorldInteraction->AddActorToExcludeFromHitTests(HMDTransformActor);

			HMDTransformActor->GetRootComponent()->SetVisibility( bIsCalibrating );
		}
	}



	// If we're actually using VR, go ahead and disable notifications.  We won't be able to see them in VR
	// currently, and they can introduce performance issues if they pop up on the desktop
	if( bActuallyUsingVR )
	{
		FSlateNotificationManager::Get().SetAllowNotifications( false );
	}

	// Setup sub systems
	{
		// Setup world interaction
		// We need input preprocessing for VR so that we can receive motion controller input without any viewports having 
		// to be focused.  This is mainly because Slate UI injected into the 3D world can cause focus to be lost unexpectedly,
		// but we need the user to still be able to interact with UI.
		WorldInteraction->SetUseInputPreprocessor( true );

		// Motion controllers
		{
			LeftHandInteractor = NewObject<UVREditorMotionControllerInteractor>();
			LeftHandInteractor->SetControllerHandSide( EControllerHand::Left );
			LeftHandInteractor->Init( this );
			WorldInteraction->AddInteractor( LeftHandInteractor );

			RightHandInteractor = NewObject<UVREditorMotionControllerInteractor>();
			RightHandInteractor->SetControllerHandSide( EControllerHand::Right );
			RightHandInteractor->Init( this );
			WorldInteraction->AddInteractor( RightHandInteractor );

			WorldInteraction->PairInteractors( LeftHandInteractor, RightHandInteractor );
		}

		if( bActuallyUsingVR )
		{
			// When actually using VR devices, we don't want a mouse cursor interactor
			WorldInteraction->ReleaseMouseCursorInteractor();
		}

		// Setup the UI system
		UISystem = NewObject<UVREditorUISystem>();
		UISystem->Init(this);

		PlacementSystem = NewObject<UVREditorPlacement>();
		PlacementSystem->Init(this);

		// Setup teleporter
		TeleportActor = SpawnTransientSceneActor<AVREditorTeleporter>( TEXT("Teleporter"), true );
		TeleportActor->Init( this );
		WorldInteraction->AddActorToExcludeFromHitTests( TeleportActor );	

		// Setup autoscaler
		AutoScalerSystem = NewObject<UVREditorAutoScaler>();
		AutoScalerSystem->Init( this );

		LeftHandInteractor->SetupComponent( AvatarActor );
		RightHandInteractor->SetupComponent( AvatarActor );
	}


	/** This will make sure this is not ticking after the editor has been closed. */
	GEditor->OnEditorClose().AddUObject( this, &UVREditorMode::OnEditorClosed );

	// Load calibration offset from disk
	{
		const TCHAR* EditorVRModeSettings = TEXT( "EditorVRMode" );
		FVector CalLocation;
		FRotator CalRotation;
		if( GConfig->GetVector( EditorVRModeSettings, TEXT( "CalLocation" ), CalLocation, GEditorSettingsIni ) &&
			GConfig->GetRotator( EditorVRModeSettings, TEXT( "CalRotation" ), CalRotation, GEditorSettingsIni ) )
		{
			CalibrationTransformOffset = FTransform( CalRotation, CalLocation, FVector::OneVector );
			UE_LOG( LogCore, Log, 
				TEXT( "PANDA:  LOADED calibration offset:  %s, %s.  Room space head transform is: %s, %s" ), 
				*CalLocation.ToString(), *CalRotation.ToString(), 
				*GetRoomSpaceHeadTransform().GetLocation().ToString(), *GetRoomSpaceHeadTransform().GetRotation().Rotator().ToString() );
		}
	}


	bFirstTick = true;
	SetActive(true);
	bStartedPlayFromVREditor = false;
	bStartedPlayFromVREditorSimulate = false;
}

void UVREditorMode::Exit(const bool bShouldDisableStereo)
{
	const FVector CalLocation = CalibrationTransformOffset.GetLocation();
	const FRotator CalRotation = CalibrationTransformOffset.GetRotation().Rotator();
	UE_LOG( LogCore, Log,
		TEXT( "PANDA:  EXITING VR.  Calibration offset is:  %s, %s.  Room space head transform is: %s, %s" ),
		*CalLocation.ToString(), *CalRotation.ToString(),
		*GetRoomSpaceHeadTransform().GetLocation().ToString(), *GetRoomSpaceHeadTransform().GetRotation().Rotator().ToString() );

	IConsoleManager::Get().UnregisterConsoleObject( CalibrateConsoleCommand );

	{
		FVREditorActionCallbacks::ChangeEditorModes(FBuiltinEditorModes::EM_Placement);

		//Destroy the avatar
		{
			DestroyTransientActor(AvatarActor);
			AvatarActor = nullptr;
			FlashlightComponent = nullptr;
		}

		//@TODO VREDITOR DEMO HACKS
		{
			RoomSpacePivotActor->Destroy();
			RoomSpacePivotActor = nullptr;
			HMDTransformActor->Destroy();
			HMDTransformActor = nullptr;
			CameraBaseActor->Destroy();
			CameraBaseActor = nullptr;
		}

		{
			if(bActuallyUsingVR)
			{
				// Restore Slate drag trigger distance
				FSlateApplication::Get().SetDragTriggerDistance( SavedEditorState.DragTriggerDistance );

				// Restore gizmo size
				WorldInteraction->SetTransformGizmoScale( SavedEditorState.TransformGizmoScale );
				WorldInteraction->SetShouldSuppressExistingCursor(false);

				// Take note of VREditor exiting (only if actually in VR)
				if (FEngineAnalytics::IsAvailable())
				{
					FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.ExitVRMode"));
				}
			}

			CloseViewport( bShouldDisableStereo );

			VREditorLevelViewportWeakPtr.Reset();
			OnVREditingModeExit_Handler.ExecuteIfBound();
		}

		// Kill the VR editor window
		TSharedPtr<SWindow> VREditorWindow( VREditorWindowWeakPtr.Pin() );
		if(VREditorWindow.IsValid())
		{
			VREditorWindow->RequestDestroyWindow();
			VREditorWindow.Reset();
		}
	}

	// Kill subsystems
	if( UISystem != nullptr )
	{
		UISystem->Shutdown();
		UISystem->MarkPendingKill();
		UISystem = nullptr;
	}

	if( PlacementSystem != nullptr )
	{
		PlacementSystem->Shutdown();
		PlacementSystem->MarkPendingKill();
		PlacementSystem = nullptr;
	}

	if( TeleportActor != nullptr )
	{
		DestroyTransientActor( TeleportActor );
		TeleportActor = nullptr;
	}

	if( AutoScalerSystem != nullptr )
	{
		AutoScalerSystem->Shutdown();
		AutoScalerSystem->MarkPendingKill();
		AutoScalerSystem = nullptr;
	}

	if( WorldInteraction != nullptr )
	{
		WorldInteraction->SetUseInputPreprocessor( false );

		WorldInteraction->OnHandleKeyInput().RemoveAll( this );
		WorldInteraction->OnPreWorldInteractionTick().RemoveAll( this );
		WorldInteraction->OnPostWorldInteractionTick().RemoveAll( this );

		WorldInteraction->RemoveInteractor( LeftHandInteractor );
		LeftHandInteractor->MarkPendingKill();
		LeftHandInteractor->Shutdown();
		LeftHandInteractor = nullptr;

		WorldInteraction->RemoveInteractor( RightHandInteractor );
		RightHandInteractor->Shutdown();
		RightHandInteractor->MarkPendingKill();
		RightHandInteractor = nullptr;
		
		// Restore the mouse cursor if we removed it earlier
		if( bActuallyUsingVR )
		{
			WorldInteraction->AddMouseCursorInteractor();
			WorldInteraction->SetInVR(false);
		}

		WorldInteraction->SetDefaultOptionalViewportClient( nullptr );
	}

	if( bActuallyUsingVR )
	{
		FSlateNotificationManager::Get().SetAllowNotifications( true);
	}

	FEditorDelegates::PostPIEStarted.RemoveAll( this );
	FEditorDelegates::PrePIEEnded.RemoveAll( this );
	FEditorDelegates::EndPIE.RemoveAll( this );
	FEditorDelegates::OnPreSwitchBeginPIEAndSIE.RemoveAll(this);
	FEditorDelegates::OnSwitchBeginPIEAndSIE.RemoveAll(this);

	GEditor->OnEditorClose().RemoveAll( this );

	if (GEditor->bIsSimulatingInEditor)
	{
		GEditor->RequestEndPlayMap();
	}

	bWantsToExitMode = false;
	SetActive(false);
	bFirstTick = false;
}

void UVREditorMode::OnEditorClosed()
{
	if(IsActive())
	{
		Exit( false );
		Shutdown();
	}
}

void UVREditorMode::StartExitingVRMode()
{
	bWantsToExitMode = true;
}

void UVREditorMode::OnVREditorWindowClosed( const TSharedRef<SWindow>& ClosedWindow )
{
	StartExitingVRMode();
}

void UVREditorMode::PreTick( const float DeltaTime )
{
	if( !bIsFullyInitialized || !IsActive() || bWantsToExitMode )
	{
		return;
	}

	//Setting the initial position and rotation based on the editor viewport when going into VR mode
	if( bFirstTick && bActuallyUsingVR )
	{
		const FTransform RoomToWorld = GetRoomTransform();
		const FTransform WorldToRoom = RoomToWorld.Inverse();
		FTransform ViewportToWorld = FTransform( SavedEditorState.ViewRotation, SavedEditorState.ViewLocation );
		FTransform ViewportToRoom = ( ViewportToWorld * WorldToRoom );

		FTransform ViewportToRoomYaw = ViewportToRoom;
		ViewportToRoomYaw.SetRotation( FQuat( FRotator( 0.0f, ViewportToRoomYaw.GetRotation().Rotator().Yaw, 0.0f ) ) );

		FTransform HeadToRoomYaw = GetRoomSpaceHeadTransform();
		HeadToRoomYaw.SetRotation( FQuat( FRotator( 0.0f, HeadToRoomYaw.GetRotation().Rotator().Yaw, 0.0f ) ) );

		FTransform RoomToWorldYaw = RoomToWorld;
		RoomToWorldYaw.SetRotation( FQuat( FRotator( 0.0f, RoomToWorldYaw.GetRotation().Rotator().Yaw, 0.0f ) ) );

		FTransform ResultToWorld = ( HeadToRoomYaw.Inverse() * ViewportToRoomYaw ) * RoomToWorldYaw;
		SetRoomTransform( ResultToWorld );
	}
}

void UVREditorMode::PostTick( float DeltaTime )
{
	if( !bIsFullyInitialized || !IsActive() || bWantsToExitMode || !VREditorLevelViewportWeakPtr.IsValid() )
	{
		return;
	}

	TickHandle.Broadcast( DeltaTime );
	UISystem->Tick( GetLevelViewportPossessedForVR().GetViewportClient().Get(), DeltaTime );

	// Update avatar meshes
	{
		// Move our avatar mesh along with the room.  We need our hand components to remain the same coordinate space as the 
		AvatarActor->SetActorTransform( GetRoomTransform() );
		AvatarActor->TickManually( DeltaTime );


	}

	//@TODO VR EDITOR DEMO HACKS
	{
		const FTransform RoomToWorld = GetRoomTransform();
		RoomSpacePivotActor->SetActorTransform( RoomToWorld );
		RoomSpacePivotActor->ForceNetUpdate();

		HMDTransformActor->SetActorTransform( GetHeadTransform() );
		HMDTransformActor->ForceNetUpdate();

		FTransform TargetToRoom = CalibrationTransformOffset;
		FTransform WorldScaleTransform = FTransform( FRotator::ZeroRotator, FVector::ZeroVector, FVector( GetWorldScaleFactor() ) );
		FTransform TargetToScaledRoom = TargetToRoom * WorldScaleTransform;

		FTransform RoomToTarget = TargetToScaledRoom.Inverse();

		FTransform WorldToRoom = RoomToWorld.Inverse();

		FTransform WorldToTarget = WorldToRoom * RoomToTarget;
		FTransform TargetToWorld = WorldToTarget.Inverse();
        

		CameraBaseActor->SetActorTransform( TargetToWorld );
		CameraBaseActor->ForceNetUpdate();
	}

	// Updating the scale and intensity of the flashlight according to the world scale
	if (FlashlightComponent)
	{
		float CurrentFalloffExponent = FlashlightComponent->LightFalloffExponent;
		//@todo vreditor tweak
		float UpdatedFalloffExponent = FMath::Clamp(CurrentFalloffExponent / GetWorldScaleFactor(), 2.0f, 16.0f);
		FlashlightComponent->SetLightFalloffExponent(UpdatedFalloffExponent);
	}

	if( bIsCalibrating && IsActuallyUsingVR() )
	{
		const FTransform RoomSpaceHeadToWorld = WorldInteraction->GetRoomSpaceHeadTransform();
		const float WorldScaleFactor = WorldInteraction->GetWorldScaleFactor();

		const float InnerRadius = 2.0f * WorldScaleFactor;
		const float OuterRadius = 4.0f * WorldScaleFactor;

		const FTransform HeadToWorld = WorldInteraction->GetHeadTransform();

		{
			const FTransform UIToHeadTransform = FTransform(
				FRotator::ZeroRotator,
				FVector( 20.0f, 0.0f, 15.0f ) * WorldScaleFactor );
			const FTransform UIToWorld = UIToHeadTransform * HeadToWorld;
			const float Thickness = 0.5f;
			DrawDebug2DDonut(
				GetWorld(),
				UIToWorld.ToMatrixNoScale(),
				InnerRadius,
				OuterRadius,
				64,
				FColor( 140, 140, 255 ),
				false,
				0.0f,
				SDPG_World,
				Thickness );
		}

		if( false )	// Doesn't work in DemoMode because messages are suppressed
		{
			GEngine->AddOnScreenDebugMessage(
				22210,
				0.0f,
				FColor( 140, 140, 255 ),
				*FString::Printf( TEXT( "MR Calibration Mode" ) ), false );
			GEngine->AddOnScreenDebugMessage(
				22211,
				0.0f,
				FColor( 140, 140, 255 ),
				*FString::Printf( TEXT( "Offset: %s" ), *CalibrationTransformOffset.GetLocation().ToString() ), false );
			GEngine->AddOnScreenDebugMessage(
				22212,
				0.0f,
				FColor( 140, 140, 255 ),
				*FString::Printf( TEXT( "Orient: %s" ), *CalibrationTransformOffset.GetRotation().Rotator().ToString() ), false );
		}
	}

	if( WorldInteraction->HaveHeadTransform() && VREd::ShowHeadVelocity->GetInt() != 0 )
	{
		const FTransform RoomSpaceHeadToWorld = WorldInteraction->GetRoomSpaceHeadTransform();
		static FTransform LastRoomSpaceHeadToWorld = RoomSpaceHeadToWorld;

		const float WorldScaleFactor = WorldInteraction->GetWorldScaleFactor();
		static float LastWorldScaleFactor = WorldScaleFactor;

		const float MinInnerRadius = VREd::HeadVelocityMinRadius->GetFloat() * WorldScaleFactor;
		const float MaxOuterRadius = VREd::HeadVelocityMaxRadius->GetFloat() * WorldScaleFactor;
		const float MinThickness = VREd::HeadVelocityMinLineThickness->GetFloat() * WorldScaleFactor;
		const float MaxThickness = VREd::HeadVelocityMaxLineThickness->GetFloat() * WorldScaleFactor;

		const float MaxLocationVelocity = VREd::HeadLocationMaxVelocity->GetFloat();	// cm/s
		const float MaxRotationVelocity = VREd::HeadRotationMaxVelocity->GetFloat();	// degrees/s

		const float LocationVelocity = ( LastRoomSpaceHeadToWorld.GetLocation() / LastWorldScaleFactor - RoomSpaceHeadToWorld.GetLocation() / WorldScaleFactor ).Size() / DeltaTime;

		const float YawVelocity = FMath::Abs( FMath::FindDeltaAngleDegrees( LastRoomSpaceHeadToWorld.GetRotation().Rotator().Yaw, RoomSpaceHeadToWorld.GetRotation().Rotator().Yaw ) ) / DeltaTime;
		const float PitchVelocity = FMath::Abs( FMath::FindDeltaAngleDegrees( LastRoomSpaceHeadToWorld.GetRotation().Rotator().Pitch, RoomSpaceHeadToWorld.GetRotation().Rotator().Pitch ) ) / DeltaTime;
		const float RollVelocity = FMath::Abs( FMath::FindDeltaAngleDegrees( LastRoomSpaceHeadToWorld.GetRotation().Rotator().Roll, RoomSpaceHeadToWorld.GetRotation().Rotator().Roll ) ) / DeltaTime;
		const float RotationVelocity = YawVelocity + PitchVelocity + RollVelocity;

		static float LastLocationVelocity = LocationVelocity;
		static float LastRotationVelocity = RotationVelocity;

		const float SmoothLocationVelocity = FMath::Lerp( LocationVelocity, LastLocationVelocity, VREd::HeadVelocitySmoothing->GetFloat() );
		const float SmoothRotationVelocity = FMath::Lerp( RotationVelocity, LastRotationVelocity, VREd::HeadVelocitySmoothing->GetFloat() );

		LastLocationVelocity = SmoothLocationVelocity;
		LastRotationVelocity = SmoothRotationVelocity;
		
		LastRoomSpaceHeadToWorld = RoomSpaceHeadToWorld;
		LastWorldScaleFactor = WorldScaleFactor;

		const float LocationVelocityAlpha = FMath::Clamp( SmoothLocationVelocity / MaxLocationVelocity, 0.0f, 1.0f );
		const float RotationVelocityAlpha = FMath::Clamp( SmoothRotationVelocity / MaxRotationVelocity, 0.0f, 1.0f );

		const FTransform HeadToWorld = WorldInteraction->GetHeadTransform();

		{
			FVector HeadLocationVelocityOffset = FVector::ZeroVector;
			HeadLocationVelocityOffset.InitFromString( VREd::HeadLocationVelocityOffset->GetString() );
			HeadLocationVelocityOffset *= WorldScaleFactor;

			const FColor Color = FColor::MakeFromColorTemperature( 6000.0f - LocationVelocityAlpha * 5000.0f );
			const float Thickness = FMath::Lerp( MinThickness, MaxThickness, LocationVelocityAlpha );
			const FTransform UIToHeadTransform = FTransform( FRotator( 0.0f, 0.0f, 0.0f ).Quaternion(), HeadLocationVelocityOffset );
			const FTransform UIToWorld = UIToHeadTransform * HeadToWorld;
			DrawDebug2DDonut( GetWorld(), UIToWorld.ToMatrixNoScale(), MinInnerRadius, FMath::Lerp( MinInnerRadius, MaxOuterRadius, LocationVelocityAlpha ), 64, Color, false, 0.0f, SDPG_World, Thickness );
		}

		{
			FVector HeadRotationVelocityOffset = FVector::ZeroVector;
			HeadRotationVelocityOffset.InitFromString( VREd::HeadRotationVelocityOffset->GetString() );
			HeadRotationVelocityOffset *= WorldScaleFactor;

			const FColor Color = FColor::MakeFromColorTemperature( 6000.0f - RotationVelocityAlpha * 5000.0f );
			const float Thickness = FMath::Lerp( MinThickness, MaxThickness, RotationVelocityAlpha );
			const FTransform UIToHeadTransform = FTransform( FRotator( 0.0f, 0.0f, 0.0f ).Quaternion(), HeadRotationVelocityOffset );
			const FTransform UIToWorld = UIToHeadTransform * HeadToWorld;
			DrawDebug2DDonut( GetWorld(), UIToWorld.ToMatrixNoScale(), MinInnerRadius, FMath::Lerp( MinInnerRadius, MaxOuterRadius, RotationVelocityAlpha ), 64, Color, false, 0.0f, SDPG_World, Thickness );
		}
	}

	bFirstTick = false;
}

FTransform UVREditorMode::GetRoomTransform() const
{
	return WorldInteraction->GetRoomTransform();
}

void UVREditorMode::SetRoomTransform( const FTransform& NewRoomTransform )
{
	WorldInteraction->SetRoomTransform( NewRoomTransform );
}

FTransform UVREditorMode::GetRoomSpaceHeadTransform() const
{
	return WorldInteraction->GetRoomSpaceHeadTransform();
}

FTransform UVREditorMode::GetHeadTransform() const
{
	return WorldInteraction->GetHeadTransform();
}

const UViewportWorldInteraction& UVREditorMode::GetWorldInteraction() const
{
	return *WorldInteraction;
}

UViewportWorldInteraction& UVREditorMode::GetWorldInteraction()
{
	return *WorldInteraction;
}

bool UVREditorMode::IsFullyInitialized() const
{
	return bIsFullyInitialized;
}

bool UVREditorMode::IsShowingRadialMenu(const UVREditorInteractor* Interactor) const
{
	return UISystem->IsShowingRadialMenu(Interactor);
}

const SLevelViewport& UVREditorMode::GetLevelViewportPossessedForVR() const
{
	return *VREditorLevelViewportWeakPtr.Pin();
}

SLevelViewport& UVREditorMode::GetLevelViewportPossessedForVR()
{
	return *VREditorLevelViewportWeakPtr.Pin();
}


float UVREditorMode::GetWorldScaleFactor() const
{
	return WorldInteraction->GetWorldScaleFactor();
}

void UVREditorMode::ToggleFlashlight( UVREditorInteractor* Interactor )
{
	UVREditorMotionControllerInteractor* MotionControllerInteractor = Cast<UVREditorMotionControllerInteractor>( Interactor );
	if ( MotionControllerInteractor )
	{
		if ( FlashlightComponent == nullptr )
		{
			FlashlightComponent = NewObject<USpotLightComponent>( AvatarActor );
			AvatarActor->AddOwnedComponent( FlashlightComponent );
			FlashlightComponent->RegisterComponent();
			FlashlightComponent->SetMobility( EComponentMobility::Movable );
			FlashlightComponent->SetCastShadows( false );
			FlashlightComponent->bUseInverseSquaredFalloff = false;
			//@todo vreditor tweak
			FlashlightComponent->SetLightFalloffExponent( 8.0f );
			FlashlightComponent->SetIntensity( 20.0f );
			FlashlightComponent->SetOuterConeAngle( 25.0f );
			FlashlightComponent->SetInnerConeAngle( 25.0f );

		}

		const FAttachmentTransformRules AttachmentTransformRules = FAttachmentTransformRules( EAttachmentRule::KeepRelative, true );
		FlashlightComponent->AttachToComponent( MotionControllerInteractor->GetMotionControllerComponent(), AttachmentTransformRules );
		bIsFlashlightOn = !bIsFlashlightOn;
		FlashlightComponent->SetVisibility( bIsFlashlightOn );
	}
}

void UVREditorMode::CycleTransformGizmoHandleType()
{
	EGizmoHandleTypes NewGizmoType = (EGizmoHandleTypes)( (uint8)WorldInteraction->GetCurrentGizmoType() + 1 );
	
	if( NewGizmoType > EGizmoHandleTypes::Scale )
	{
		NewGizmoType = EGizmoHandleTypes::All;
	}

	WorldInteraction->SetGizmoHandleType( NewGizmoType );
}

EHMDDeviceType::Type UVREditorMode::GetHMDDeviceType() const
{
	return GEngine->HMDDevice.IsValid() ? GEngine->HMDDevice->GetHMDDeviceType() : EHMDDeviceType::DT_SteamVR;
}

FLinearColor UVREditorMode::GetColor( const EColors Color ) const
{
	return Colors[ (int32)Color ];
}

float UVREditorMode::GetDefaultVRNearClipPlane() const 
{
	return VREd::DefaultVRNearClipPlane->GetFloat();
}

void UVREditorMode::RefreshVREditorSequencer(class ISequencer* InCurrentSequencer)
{
	CurrentSequencer = InCurrentSequencer;
	// Tell the VR Editor UI system to refresh the Sequencer UI
	if (UISystem != nullptr && bActuallyUsingVR)
	{
		GetUISystem().UpdateSequencerUI();
	}
}

class ISequencer* UVREditorMode::GetCurrentSequencer()
{
	return CurrentSequencer;
}

bool UVREditorMode::IsHandAimingTowardsCapsule(UViewportInteractor* Interactor, const FTransform& CapsuleTransform, FVector CapsuleStart, const FVector CapsuleEnd, const float CapsuleRadius, const float MinDistanceToCapsule, const FVector CapsuleFrontDirection, const float MinDotForAimingAtCapsule) const
{
	bool bIsAimingTowards = false;
	const float WorldScaleFactor = GetWorldScaleFactor();

	FVector LaserPointerStart, LaserPointerEnd;
	if( Interactor->GetLaserPointer( /* Out */ LaserPointerStart, /* Out */ LaserPointerEnd ) )
	{
		const FVector LaserPointerStartInCapsuleSpace = CapsuleTransform.InverseTransformPosition( LaserPointerStart );
		const FVector LaserPointerEndInCapsuleSpace = CapsuleTransform.InverseTransformPosition( LaserPointerEnd );

		FVector ClosestPointOnLaserPointer, ClosestPointOnUICapsule;
		FMath::SegmentDistToSegment(
			LaserPointerStartInCapsuleSpace, LaserPointerEndInCapsuleSpace,
			CapsuleStart, CapsuleEnd,
			/* Out */ ClosestPointOnLaserPointer,
			/* Out */ ClosestPointOnUICapsule );

		const bool bIsClosestPointInsideCapsule = ( ClosestPointOnLaserPointer - ClosestPointOnUICapsule ).Size() <= CapsuleRadius;

		const FVector TowardLaserPointerVector = ( ClosestPointOnLaserPointer - ClosestPointOnUICapsule ).GetSafeNormal();

		// Apply capsule radius
		ClosestPointOnUICapsule += TowardLaserPointerVector * CapsuleRadius;

		if( false )	// @todo vreditor debug
		{
			const float RenderCapsuleLength = ( CapsuleEnd - CapsuleStart ).Size() + CapsuleRadius * 2.0f;
			// @todo vreditor:  This capsule draws with the wrong orientation
			if( false )
			{
				DrawDebugCapsule( GetWorld(), CapsuleTransform.TransformPosition( CapsuleStart + ( CapsuleEnd - CapsuleStart ) * 0.5f ), RenderCapsuleLength * 0.5f, CapsuleRadius, CapsuleTransform.GetRotation() * FRotator( 90.0f, 0, 0 ).Quaternion(), FColor::Green, false, 0.0f );
			}
			DrawDebugLine( GetWorld(), CapsuleTransform.TransformPosition( ClosestPointOnLaserPointer ), CapsuleTransform.TransformPosition( ClosestPointOnUICapsule ), FColor::Green, false, 0.0f );
			DrawDebugSphere( GetWorld(), CapsuleTransform.TransformPosition( ClosestPointOnLaserPointer ), 1.5f * WorldScaleFactor, 32, FColor::Red, false, 0.0f );
			DrawDebugSphere( GetWorld(), CapsuleTransform.TransformPosition( ClosestPointOnUICapsule ), 1.5f * WorldScaleFactor, 32, FColor::Green, false, 0.0f );
		}

		// If we're really close to the capsule
		if( bIsClosestPointInsideCapsule ||
			( ClosestPointOnUICapsule - ClosestPointOnLaserPointer ).Size() <= MinDistanceToCapsule )
		{
			const FVector LaserPointerDirectionInCapsuleSpace = ( LaserPointerEndInCapsuleSpace - LaserPointerStartInCapsuleSpace ).GetSafeNormal();

			if( false )	// @todo vreditor debug
			{
				DrawDebugLine( GetWorld(), CapsuleTransform.TransformPosition( FVector::ZeroVector ), CapsuleTransform.TransformPosition( CapsuleFrontDirection * 5.0f ), FColor::Yellow, false, 0.0f );
				DrawDebugLine( GetWorld(), CapsuleTransform.TransformPosition( FVector::ZeroVector ), CapsuleTransform.TransformPosition( -LaserPointerDirectionInCapsuleSpace * 5.0f ), FColor::Purple, false, 0.0f );
			}

			const float Dot = FVector::DotProduct( CapsuleFrontDirection, -LaserPointerDirectionInCapsuleSpace );
			if( Dot >= MinDotForAimingAtCapsule )
			{
				bIsAimingTowards = true;
			}
		}
	}

	return bIsAimingTowards;
}

UVREditorInteractor* UVREditorMode::GetHandInteractor( const EControllerHand ControllerHand ) const 
{
	UVREditorInteractor* ResultInteractor = ControllerHand == EControllerHand::Left ? LeftHandInteractor : RightHandInteractor;
	check( ResultInteractor != nullptr );
	return ResultInteractor;
}

void UVREditorMode::SnapSelectedActorsToGround()
{
	TSharedPtr<SLevelViewport> LevelEditorViewport = StaticCastSharedPtr<SLevelViewport>(WorldInteraction->GetDefaultOptionalViewportClient()->GetEditorViewportWidget());
	if (LevelEditorViewport.IsValid())
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		const FLevelEditorCommands& Commands = LevelEditorModule.GetLevelEditorCommands();
		const TSharedPtr< FUICommandList >& CommandList = LevelEditorViewport->GetParentLevelEditor().Pin()->GetLevelEditorActions(); //@todo vreditor: Cast on leveleditor

		CommandList->ExecuteAction(Commands.SnapBottomCenterBoundsToFloor.ToSharedRef());

		// Force transformables to refresh
		GEditor->NoteSelectionChange();
	}
}

const UVREditorMode::FSavedEditorState& UVREditorMode::GetSavedEditorState() const
{
	return SavedEditorState;
}

void UVREditorMode::SaveSequencerSettings(bool bInKeyAllEnabled, EAutoKeyMode InAutoKeyMode, const class USequencerSettings& InSequencerSettings)
{
	SavedEditorState.bKeyAllEnabled = bInKeyAllEnabled;
	SavedEditorState.AutoKeyMode = InAutoKeyMode;
}

void UVREditorMode::ToggleSIEAndVREditor()
{
	if (GEditor->EditorWorld == nullptr && !GEditor->bIsSimulatingInEditor)
	{
		const FVector* StartLoc = NULL;
		const FRotator* StartRot = NULL;
		GEditor->RequestPlaySession(false, VREditorLevelViewportWeakPtr.Pin(), true /*bSimulateInEditor*/, StartLoc, StartRot, -1);
	}
	else if (GEditor->PlayWorld != nullptr && GEditor->bIsSimulatingInEditor)
	{
		GEditor->RequestEndPlayMap();
	}
}

void UVREditorMode::TogglePIEAndVREditor()
{
	bool bRequestedPIE = false;
	if (GEditor->EditorWorld == nullptr && GEditor->PlayWorld == nullptr && !GEditor->bIsSimulatingInEditor)
	{
		const FVector* StartLoc = NULL;
		const FRotator* StartRot = NULL;
		const bool bHMDIsReady = (GEngine && GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHMDConnected());
		GEditor->RequestPlaySession(true, VREditorLevelViewportWeakPtr.Pin(), false /*bSimulateInEditor*/, StartLoc, StartRot, -1, false, bHMDIsReady);
		bRequestedPIE = true;
	}
	else if (GEditor->PlayWorld != nullptr)
	{
		// Since we are already in simulate, we want to toggle to PIE.
		if (GEditor->bIsSimulatingInEditor)
		{
			bStartedPlayFromVREditorSimulate = true;
			bRequestedPIE = true;

			GEditor->RequestToggleBetweenPIEandSIE();
		}
		else
		{
			// If this play started while in simulate, then toggle back to simulate.
			if (bStartedPlayFromVREditorSimulate)
			{
				GEditor->RequestToggleBetweenPIEandSIE();
			}
			else
			{
				GEditor->RequestEndPlayMap();
			}
		}
	}

	if (bRequestedPIE)
	{
		// Turn off input processing while in PIE.  We don't want any input events until the user comes back to the editor
		WorldInteraction->SetUseInputPreprocessor(false);

		SavedWorldToMetersScaleForPIE = GetWorld()->GetWorldSettings()->WorldToMeters;

		// Restore the world to meters before entering play
		RestoreWorldToMeters();

		SetActive(false);
		WorldInteraction->SetActive(false);
		bStartedPlayFromVREditor = true;
	}
}

void UVREditorMode::TransitionWorld(UWorld* NewWorld)
{
	Super::TransitionWorld(NewWorld);

	UISystem->TransitionWorld(NewWorld);
}

void UVREditorMode::StartViewport(TSharedPtr<SLevelViewport> Viewport)
{
	if (false)
	{
		const TSharedRef< ILevelEditor >& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor").GetFirstLevelEditor().ToSharedRef();

		// @todo vreditor: The resolution we set here doesn't matter, as HMDs will draw at their native resolution
		// no matter what.  We should probably allow the window to be freely resizable by the user
		// @todo vreditor: Should save and restore window position and size settings
		FVector2D WindowSize;
		{
			IHeadMountedDisplay::MonitorInfo HMDMonitorInfo;
			if (bActuallyUsingVR && GEngine->HMDDevice->GetHMDMonitorInfo(HMDMonitorInfo))
			{
				WindowSize = FVector2D(HMDMonitorInfo.ResolutionX, HMDMonitorInfo.ResolutionY);
			}
			else
			{
				// @todo vreditor: Hard-coded failsafe window size
				WindowSize = FVector2D(1920.0f, 1080.0f);
			}
		}

		// @todo vreditor: Use SLevelEditor::GetTableTitle() for the VR window title (needs dynamic update)
		const FText VREditorWindowTitle = NSLOCTEXT("VREditor", "VRWindowTitle", "Unreal Editor VR");

		TSharedRef< SWindow > VREditorWindow = SNew(SWindow)
			.Title(VREditorWindowTitle)
			.ClientSize(WindowSize)
			.AutoCenter(EAutoCenter::PreferredWorkArea)
			.UseOSWindowBorder(true)	// @todo vreditor: Allow window to be freely resized?  Shouldn't really hurt anything.  We should save position/size too.
			.SizingRule(ESizingRule::UserSized);
		this->VREditorWindowWeakPtr = VREditorWindow;

		Viewport =
			SNew(SLevelViewport)
			.ViewportType(LVT_Perspective) // Perspective
			.Realtime(true)
			//				.ParentLayout( AsShared() )	// @todo vreditor: We don't have one and we probably don't need one, right?  Make sure a null parent layout is handled properly everywhere.
			.ParentLevelEditor(LevelEditor)
			//				.ConfigKey( BottomLeftKey )	// @todo vreditor: This is for saving/loading layout.  We would need this in order to remember viewport settings like show flags, etc.
			.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute());

		// Allow the editor to keep track of this editor viewport.  Because it's not inside of a normal tab, 
		// we need to explicitly tell the level editor about it
		LevelEditor->AddStandaloneLevelViewport(Viewport.ToSharedRef());

		VREditorWindow->SetContent(Viewport.ToSharedRef());

		// NOTE: We're intentionally not adding this window natively parented to the main frame window, because we don't want it
		// to minimize/restore when the main frame is minimized/restored
		FSlateApplication::Get().AddWindow(VREditorWindow);

		VREditorWindow->SetOnWindowClosed(FOnWindowClosed::CreateUObject(this, &UVREditorMode::OnVREditorWindowClosed));

		VREditorWindow->BringToFront();	// @todo vreditor: Not sure if this is needed, especially if we decide the window should be hidden (copied this from PIE code)
	}
	else
	{
		if (bActuallyUsingVR && !Viewport->IsImmersive())
		{
			// Switch to immersive mode
			const bool bWantImmersive = true;
			const bool bAllowAnimation = false;
			Viewport->MakeImmersive(bWantImmersive, bAllowAnimation);
		}
	}

	this->VREditorLevelViewportWeakPtr = Viewport;

	{
		FLevelEditorViewportClient& VRViewportClient = Viewport->GetLevelViewportClient();
		FEditorViewportClient& VREditorViewportClient = VRViewportClient;

		// Make sure we are in perspective mode
		// @todo vreditor: We should never allow ortho switching while in VR
		SavedEditorState.ViewportType = VREditorViewportClient.GetViewportType();
		VREditorViewportClient.SetViewportType(LVT_Perspective);

		// Set the initial camera location
		// @todo vreditor: This should instead be calculated using the currently active perspective camera's
		// location and orientation, compensating for the current HMD offset from the tracking space origin.
		// Perhaps, we also want to teleport the original viewport's camera back when we exit this mode, too!
		// @todo vreditor: Should save and restore camera position and any other settings we change (viewport type, pitch locking, etc.)
		SavedEditorState.ViewLocation = VRViewportClient.GetViewLocation();
		SavedEditorState.ViewRotation = VRViewportClient.GetViewRotation();

		// Don't allow the tracking space to pitch up or down.  People hate that in VR.
		// @todo vreditor: This doesn't seem to prevent people from pitching the camera with RMB drag
		SavedEditorState.bLockedPitch = VRViewportClient.GetCameraController()->GetConfig().bLockedPitch;
		if (bActuallyUsingVR)
		{
			VRViewportClient.GetCameraController()->AccessConfig().bLockedPitch = true;
		}

		// Set "game mode" to be enabled, to get better performance.  Also hit proxies won't work in VR, anyway
		SavedEditorState.bGameView = VREditorViewportClient.IsInGameView();
		VREditorViewportClient.SetGameView(true);

		SavedEditorState.bRealTime = VREditorViewportClient.IsRealtime();
		VREditorViewportClient.SetRealtime(true);

		SavedEditorState.ShowFlags = VREditorViewportClient.EngineShowFlags;

		// Never show the traditional Unreal transform widget.  It doesn't work in VR because we don't have hit proxies.
		VREditorViewportClient.EngineShowFlags.SetModeWidgets(false);

		// Make sure the mode widgets don't come back when users click on things
		VRViewportClient.bAlwaysShowModeWidgetAfterSelectionChanges = false;

		// Force tiny near clip plane distance, because user can scale themselves to be very small.
		SavedEditorState.NearClipPlane = GNearClippingPlane;
		GNearClippingPlane = GetDefaultVRNearClipPlane();

		SavedEditorState.bOnScreenMessages = GAreScreenMessagesEnabled;
		GAreScreenMessagesEnabled = false;

		// Save the world to meters scale
		{
			const float DefaultWorldToMeters = VREd::DefaultWorldToMeters->GetFloat();
			const float SavedWorldToMeters = DefaultWorldToMeters != 0.0f ? DefaultWorldToMeters : VRViewportClient.GetWorld()->GetWorldSettings()->WorldToMeters;
			SavedEditorState.WorldToMetersScale = SavedWorldToMeters;
			SavedWorldToMetersScaleForPIE = SavedWorldToMeters;
		}

		if (bActuallyUsingVR)
		{
			SavedEditorState.TrackingOrigin = GEngine->HMDDevice->GetTrackingOrigin();
			GEngine->HMDDevice->SetTrackingOrigin(EHMDTrackingOrigin::Floor);
		}

		// Make the new viewport the active level editing viewport right away
		GCurrentLevelEditingViewportClient = &VRViewportClient;

		// Enable selection outline right away
		VREditorViewportClient.EngineShowFlags.SetSelection(true);
		VREditorViewportClient.EngineShowFlags.SetSelectionOutline(true);

		// Change viewport settings to more VR-friendly sequencer settings
		SavedEditorState.bCinematicPreviewViewport = VRViewportClient.AllowsCinematicPreview();
		VRViewportClient.SetAllowCinematicPreview(false);
		// Need to force fading and color scaling off in case we enter VR editing mode with a sequence open
		if (VREd::AllowVRModeFading->GetInt() == 0)
		{
			VRViewportClient.bEnableFading = false;
			VRViewportClient.SetAllowCinematicPreview( true );
		}
		VRViewportClient.bEnableColorScaling = false;
		VRViewportClient.Invalidate(true);
	}

	if (bActuallyUsingVR)
	{
		Viewport->EnableStereoRendering( bActuallyUsingVR );
		Viewport->SetRenderDirectlyToWindow( bActuallyUsingVR );

		GEngine->HMDDevice->EnableStereo(true);
	}

	if (WorldInteraction != nullptr)
	{
		TSharedPtr<FEditorViewportClient> VRViewportClient = Viewport->GetViewportClient();
		WorldInteraction->SetDefaultOptionalViewportClient(VRViewportClient);
	}
}

void UVREditorMode::CloseViewport( const bool bShouldDisableStereo )
{
	if (bActuallyUsingVR && GEngine->HMDDevice.IsValid() && bShouldDisableStereo)
	{
		GEngine->HMDDevice->EnableStereo(false);
	}

	TSharedPtr<SLevelViewport> VREditorLevelViewport(VREditorLevelViewportWeakPtr.Pin());
	if (VREditorLevelViewport.IsValid())
	{
		if( bShouldDisableStereo && bActuallyUsingVR )
		{
			VREditorLevelViewport->EnableStereoRendering(false);
			VREditorLevelViewport->SetRenderDirectlyToWindow(false);
		}

		{
			FLevelEditorViewportClient& VRViewportClient = VREditorLevelViewport->GetLevelViewportClient();
			FEditorViewportClient& VREditorViewportClient = VRViewportClient;

			// Restore settings that we changed on the viewport
			VREditorViewportClient.SetViewportType(SavedEditorState.ViewportType);
			VRViewportClient.GetCameraController()->AccessConfig().bLockedPitch = SavedEditorState.bLockedPitch;
			VRViewportClient.bAlwaysShowModeWidgetAfterSelectionChanges = SavedEditorState.bAlwaysShowModeWidgetAfterSelectionChanges;
			VRViewportClient.EngineShowFlags = SavedEditorState.ShowFlags;
			VRViewportClient.SetGameView(SavedEditorState.bGameView);
			VRViewportClient.SetAllowCinematicPreview(SavedEditorState.bCinematicPreviewViewport);
			VRViewportClient.bEnableFading = true;
			VRViewportClient.bEnableColorScaling = true;
			VRViewportClient.Invalidate(true);

			if (bActuallyUsingVR)
			{
				VRViewportClient.SetViewLocation(GetHeadTransform().GetLocation());

				FRotator HeadRotationNoRoll = GetHeadTransform().GetRotation().Rotator();
				HeadRotationNoRoll.Roll = 0.0f;
				VRViewportClient.SetViewRotation(HeadRotationNoRoll); // Use SavedEditorState.ViewRotation to go back to start rot
			}

			VRViewportClient.SetRealtime(SavedEditorState.bRealTime);

			GNearClippingPlane = SavedEditorState.NearClipPlane;
			GAreScreenMessagesEnabled = SavedEditorState.bOnScreenMessages;

			if (bActuallyUsingVR)
			{
				GEngine->HMDDevice->SetTrackingOrigin(SavedEditorState.TrackingOrigin);
			}

			RestoreWorldToMeters();
		}

		if (bActuallyUsingVR && bShouldDisableStereo)
		{
			// Leave immersive mode
			const bool bWantImmersive = false;
			const bool bAllowAnimation = false;
			VREditorLevelViewport->MakeImmersive(bWantImmersive, bAllowAnimation);
		}
	}
}

void UVREditorMode::RestoreFromPIE()
{
	SetActive(true);
	bStartedPlayFromVREditorSimulate = false;

	GetWorld()->GetWorldSettings()->WorldToMeters = SavedWorldToMetersScaleForPIE;
	WorldInteraction->SetWorldToMetersScale(SavedWorldToMetersScaleForPIE);

	// Re-enable input pre-processing
	WorldInteraction->SetUseInputPreprocessor(true);
	WorldInteraction->SetActive(true);

	UVREditorMotionControllerInteractor* UIInteractor = UISystem->GetUIInteractor();
	if (UIInteractor != nullptr)
	{
		UIInteractor->ResetTrackpad();
		UISystem->HideRadialMenu(false, false);
	}
}

void UVREditorMode::RestoreWorldToMeters()
{
	const float DefaultWorldToMeters = VREd::DefaultWorldToMeters->GetFloat();
	GetWorld()->GetWorldSettings()->WorldToMeters = DefaultWorldToMeters != 0.0f ? DefaultWorldToMeters : SavedEditorState.WorldToMetersScale;
	ENGINE_API extern float GNewWorldToMetersScale;
	GNewWorldToMetersScale = 0.0f;
}

UStaticMeshComponent* UVREditorMode::CreateMotionControllerMesh( AActor* OwningActor, USceneComponent* AttachmentToComponent )
{
	UStaticMesh* ControllerMesh = nullptr;
	if(GetHMDDeviceType() == EHMDDeviceType::DT_SteamVR)
	{
		ControllerMesh = AssetContainer->VivePreControllerMesh;
	}
	else if(GetHMDDeviceType() == EHMDDeviceType::DT_OculusRift)
	{
		ControllerMesh = AssetContainer->OculusControllerMesh;
	}
	else
	{
		ControllerMesh = AssetContainer->GenericControllerMesh;
	}

	return CreateMesh(OwningActor, ControllerMesh, AttachmentToComponent);
}

UStaticMeshComponent* UVREditorMode::CreateMesh( AActor* OwningActor, const FString& MeshName, USceneComponent* AttachmentToComponent /*= nullptr */ )
{
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshName);
	check(Mesh != nullptr);
	return CreateMesh(OwningActor, Mesh, AttachmentToComponent);
}

UStaticMeshComponent* UVREditorMode::CreateMesh(AActor* OwningActor, UStaticMesh* Mesh, USceneComponent* AttachmentToComponent /*= nullptr */)
{
	UStaticMeshComponent* CreatedMeshComponent = NewObject<UStaticMeshComponent>(OwningActor);
	OwningActor->AddOwnedComponent(CreatedMeshComponent);
	if (AttachmentToComponent != nullptr)
	{
		CreatedMeshComponent->SetupAttachment(AttachmentToComponent);
	}

	CreatedMeshComponent->RegisterComponent();

	CreatedMeshComponent->SetStaticMesh(Mesh);
	CreatedMeshComponent->SetMobility(EComponentMobility::Movable);
	CreatedMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CreatedMeshComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	return CreatedMeshComponent;
}

void UVREditorMode::SetActionsMenuGenerator(const FOnRadialMenuGenerated NewMenuGenerator, const FText NewLabel)
{
	GetUISystem().GetRadialMenuHandler()->SetActionsMenuGenerator(NewMenuGenerator, NewLabel);
}

void UVREditorMode::ResetActionsMenuGenerator()
{
	GetUISystem().GetRadialMenuHandler()->ResetActionsMenuGenerator();
}

void UVREditorMode::RefreshRadialMenuActionsSubmenu()
{
	GetUISystem().GetRadialMenuHandler()->RegisterMenuGenerator( GetUISystem().GetRadialMenuHandler()->GetActionsMenuGenerator() );
}

bool UVREditorMode::GetStartedPlayFromVREditor() const
{
	return bStartedPlayFromVREditor;
}

const UVREditorAssetContainer& UVREditorMode::GetAssetContainer() const
{
	return *AssetContainer;
}

void UVREditorMode::PlaySound(USoundBase* SoundBase, const FVector& InWorldLocation, const float InVolume /*= 1.0f*/)
{
	if (IsActive() && bIsFullyInitialized && GEditor != nullptr && GEditor->CanPlayEditorSound())
	{
		const float Volume = InVolume*VREd::SFXMultiplier->GetFloat();
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), SoundBase, InWorldLocation, Volume);
	}
}

bool UVREditorMode::IsAimingTeleport() const
{
	return TeleportActor->IsAiming();
}

bool UVREditorMode::IsTeleporting() const
{
	return TeleportActor->IsTeleporting();
}

void UVREditorMode::PostPIEStarted( bool bIsSimulatingInEditor )
{
	if (!bIsSimulatingInEditor)
	{
		GEnableVREditorHacks = false;
	}
}


void UVREditorMode::PrePIEEnded( bool bWasSimulatingInEditor )
{
	if (!bWasSimulatingInEditor && !bStartedPlayFromVREditorSimulate)
	{
		GEnableVREditorHacks = true;
	}
	else if (bStartedPlayFromVREditorSimulate)
	{
		// Pre PIE to SIE. When exiting play with escape, the delegate toggle PIE and SIE won't be called. We know that we started PIE from simulate. However simulate will also be closed.
		GEnableVREditorHacks = true;
	}
}

void UVREditorMode::OnEndPIE(bool bWasSimulatingInEditor)
{
	if (!bWasSimulatingInEditor && !bStartedPlayFromVREditorSimulate)
	{
		RestoreFromPIE();
	}
	else if (bStartedPlayFromVREditorSimulate)
	{
		// Post PIE to SIE
		RestoreFromPIE();
		GetOwningCollection()->ShowAllActors(true);
	}
}

void UVREditorMode::OnPreSwitchPIEAndSIE(bool bIsSimulatingInEditor)
{
	if (bStartedPlayFromVREditorSimulate)
	{
		if (bIsSimulatingInEditor)
		{
			// Pre SIE to PIE
			GetOwningCollection()->ShowAllActors(false);
		}
		else
		{
			// Pre PIE to SIE
			GEnableVREditorHacks = true;
		}
	}
}

void UVREditorMode::OnSwitchPIEAndSIE(bool bIsSimulatingInEditor)
{
	if (bStartedPlayFromVREditorSimulate)
	{
		if (bIsSimulatingInEditor)
		{
			// Post PIE to SIE
			RestoreFromPIE();
			GetOwningCollection()->ShowAllActors(true);
		}
		else
		{
			// Post SIE to PIE
			GEnableVREditorHacks = false;
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		}
	}
}


void UVREditorMode::SetIsCalibrating( const bool bNewIsCalibrating )
{
	if( bNewIsCalibrating != bIsCalibrating )
	{
		bIsCalibrating = bNewIsCalibrating;

		PlaySound( bIsCalibrating ? AssetContainer->DockableWindowOpenSound : AssetContainer->DockableWindowCloseSound, AvatarActor->GetActorLocation() );

		// If we're going into calibration mode, then make sure we're at 1.0 scale for this.
		{
			WorldInteraction->SetWorldToMetersScale( 100.0f, false );
			WorldInteraction->SkipInteractiveWorldMovementThisFrame();
		}

		RoomSpacePivotActor->GetRootComponent()->SetVisibility( bIsCalibrating );
		CameraBaseActor->GetRootComponent()->SetVisibility( bIsCalibrating );
		HMDTransformActor->GetRootComponent()->SetVisibility( bIsCalibrating );
	}
}


void UVREditorMode::DoCalibrationChange( const UVREditorMode::ECalibrationChange Change )
{
	UViewportInteractor* LaserInteractor = ( LeftHandInteractor->GetControllerType() == EControllerType::Laser ) ? LeftHandInteractor : RightHandInteractor;
	UViewportInteractor* UIInteractor = ( LeftHandInteractor->GetControllerType() == EControllerType::UI ) ? LeftHandInteractor : RightHandInteractor;

	if( Change == ECalibrationChange::Reset )
	{
		SetCalibrationTransformOffset( FTransform::Identity );
	}
	else if( Change == ECalibrationChange::SetCalibrationToLaserHand )
	{
		// Remove all roll and pitch
		FTransform TargetToRoom = LaserInteractor->GetInteractorData().RoomSpaceTransform;
		const float YawDegreesOffset = 180.0f;	// Flip 180 degrees around, the results match Vive better
		TargetToRoom.SetRotation( FRotator( 0, TargetToRoom.Rotator().Yaw + YawDegreesOffset, 0 ).Quaternion() );

		SetCalibrationTransformOffset( TargetToRoom );
	}
	else if( Change == ECalibrationChange::PositiveYaw || Change == ECalibrationChange::NegativeYaw )
	{
		// Increment or decrement yaw a little bit
		const float OldYawDegrees = CalibrationTransformOffset.GetRotation().Rotator().Yaw;
		FTransform NewCalibrationOffset = CalibrationTransformOffset;
		NewCalibrationOffset.SetRotation( FRotator( 0.0f, OldYawDegrees + ( ( Change == ECalibrationChange::NegativeYaw ) ? -0.5f : 0.5f ), 0.0f ).Quaternion() );
		SetCalibrationTransformOffset( NewCalibrationOffset );
	}
	else if(
		Change == ECalibrationChange::MoveUp ||
		Change == ECalibrationChange::MoveDown ||
		Change == ECalibrationChange::MoveLeft ||
		Change == ECalibrationChange::MoveRight ||
		Change == ECalibrationChange::MoveForward ||
		Change == ECalibrationChange::MoveBackward )
	{
		const float Amount = 0.5f;

		FVector Delta = FVector::ZeroVector;
		switch( Change )
		{
			case ECalibrationChange::MoveUp:
				Delta.Z = Amount;
				break;

			case ECalibrationChange::MoveDown:
				Delta.Z = -Amount;
				break;

			case ECalibrationChange::MoveLeft:
				Delta.Y = -Amount;
				break;

			case ECalibrationChange::MoveRight:
				Delta.Y = Amount;
				break;

			case ECalibrationChange::MoveForward:
				Delta.X = Amount;
				break;

			case ECalibrationChange::MoveBackward:
				Delta.X = -Amount;
				break;
			
			default:
				check( 0 );
		}
		const FTransform InteractorToRoom = LaserInteractor->GetInteractorData().RoomSpaceTransform;
		FTransform NewCalibrationOffset = CalibrationTransformOffset;
		NewCalibrationOffset.SetLocation( NewCalibrationOffset.GetLocation() + InteractorToRoom.TransformVectorNoScale( Delta ) );
		SetCalibrationTransformOffset( NewCalibrationOffset );
	}
}


bool UVREditorMode::InputKey( FEditorViewportClient* InViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event )
{
	bool bWasHandled = false;

	if( Event == IE_Pressed )
	{
		if( Key == EKeys::Multiply )
		{
			SetIsCalibrating( !bIsCalibrating );
			bWasHandled = true;
		}

		if( bIsCalibrating )
		{
			if( Key == EKeys::Enter )
			{
				DoCalibrationChange( ECalibrationChange::SetCalibrationToLaserHand );
				bWasHandled = true;
			}
			else if( Key == EKeys::NumPadFive )
			{
				DoCalibrationChange( ECalibrationChange::Reset );
				bWasHandled = true;
			}
			else if( Key == EKeys::NumPadSeven )
			{
				DoCalibrationChange( ECalibrationChange::NegativeYaw );
				bWasHandled = true;
			}
			else if( Key == EKeys::NumPadNine )
			{
				DoCalibrationChange( ECalibrationChange::PositiveYaw );
				bWasHandled = true;
			}
			else if( Key == EKeys::NumPadEight )
			{
				DoCalibrationChange( ECalibrationChange::MoveUp );
				bWasHandled = true;
			}
			else if( Key == EKeys::NumPadTwo )
			{
				DoCalibrationChange( ECalibrationChange::MoveDown );
				bWasHandled = true;
			}
			else if( Key == EKeys::NumPadFour )
			{
				DoCalibrationChange( ECalibrationChange::MoveLeft );
				bWasHandled = true;
			}
			else if( Key == EKeys::NumPadSix )
			{
				DoCalibrationChange( ECalibrationChange::MoveRight );
				bWasHandled = true;
			}
			else if( Key == EKeys::NumPadOne )
			{
				DoCalibrationChange( ECalibrationChange::MoveBackward );
				bWasHandled = true;
			}
			else if( Key == EKeys::NumPadThree )
			{
				DoCalibrationChange( ECalibrationChange::MoveForward );
				bWasHandled = true;
			}
		}
	}

	return bWasHandled;
}

void UVREditorMode::SetCalibrationTransformOffset( const FTransform NewOffset )
{
	CalibrationTransformOffset = NewOffset;

	// Save it to disk immediately
	{
		const TCHAR* EditorVRModeSettings = TEXT( "EditorVRMode" );
		const FVector CalLocation = CalibrationTransformOffset.GetLocation();
		const FRotator CalRotation = CalibrationTransformOffset.GetRotation().Rotator();
		GConfig->SetVector( EditorVRModeSettings, TEXT( "CalLocation" ), CalLocation, GEditorSettingsIni );
		GConfig->SetRotator( EditorVRModeSettings, TEXT( "CalRotation" ), CalRotation, GEditorSettingsIni );
		GConfig->Flush( false, GEditorSettingsIni );

		UE_LOG( LogCore, Log, 
			TEXT( "PANDA:  SETTING calibration offset to:  %s, %s.  Room space head transform is: %s, %s" ), 
			*CalLocation.ToString(), *CalRotation.ToString(), 
			*GetRoomSpaceHeadTransform().GetLocation().ToString(), *GetRoomSpaceHeadTransform().GetRotation().Rotator().ToString() );
	}
}


AVRReplicatedActor::AVRReplicatedActor()
{
	// Don't use smoothing
	this->StaticMeshComponent->bAllowReplicatedTransformSmoothing = false;

	StaticMeshComponent->Mobility = EComponentMobility::Movable;
	this->SetReplicateMovement( true );
	this->SetReplicates( true );
	this->bAlwaysRelevant = true;
	this->StaticMeshComponent->SetIsReplicated( true );

	static ConstructorHelpers::FObjectFinder<UMaterial> MaterialFinder( TEXT( "/Engine/EditorMaterials/WidgetMaterial_Z" ) );
	UMaterial* Material = MaterialFinder.Object;
	ensure( Material != nullptr );
	this->StaticMeshComponent->SetMaterial( 0, Material );
}

void AVRReplicatedActor::BeginPlay()
{
	Super::BeginPlay();
}


void ACameraBaseActor::BeginPlay()
{
	Super::BeginPlay();

	if (IsNetMode(NM_Client) || IsNetMode(NM_Standalone))
	{
		AActor* NCamActor = nullptr;
		for (FActorIterator ActorIt( GetWorld() ); ActorIt; ++ActorIt)
		{
			AActor* OtherActor = *ActorIt;
			if (OtherActor->Tags.Contains(TEXT("NcWorldOrigin")))
			{
				NCamActor = OtherActor;
				break;
			}
		}

		if( ensure( NCamActor != nullptr ) )
		{
			NCamActor->AttachToActor( this, FAttachmentTransformRules::SnapToTargetNotIncludingScale, NAME_None );
		}
	}
}


void AHMDTransformActor::BeginPlay()
{
	Super::BeginPlay();

	if( IsNetMode( NM_Client ) || IsNetMode( NM_Standalone ) )
	{
		AActor* NCamActor = nullptr;
		for( FActorIterator ActorIt( GetWorld() ); ActorIt; ++ActorIt )
		{
			AActor* OtherActor = *ActorIt;
			if( OtherActor->Tags.Contains( TEXT( "NcDepthTarget" ) ) )
			{
				NCamActor = OtherActor;
				break;
			}
		}

		if( ensure( NCamActor != nullptr ) )
		{
			NCamActor->AttachToActor( this, FAttachmentTransformRules::SnapToTargetNotIncludingScale, NAME_None );
		}
	}
}


void UVREditorMode::TeleportToTaggedActor( const TArray< FString >& Args )
{
	if( Args.Num() > 0 )
	{
		const FName ActorTag = FName( *Args[ 0 ] );
		
				
		TArray<AActor*> TaggedActors;
		UGameplayStatics::GetAllActorsWithTag( GetWorld(), ActorTag, /* Out */ TaggedActors );

		if( ensure( TaggedActors.Num() > 0 ) )
		{
			AActor* TaggedActor = TaggedActors[ 0 ];

			// Reset scale
			WorldInteraction->SetWorldToMetersScale( 100.0f, false );
			WorldInteraction->SkipInteractiveWorldMovementThisFrame();

			// Teleport.  Take into account calibration offset.
			{
				const FTransform TargetToRoom = CalibrationTransformOffset;
				const FTransform RoomActorToWorld = TaggedActor->GetActorTransform();
				const FTransform NewRoomToWorld = TargetToRoom.Inverse() * RoomActorToWorld;
				SetRoomTransform( NewRoomToWorld );
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE
