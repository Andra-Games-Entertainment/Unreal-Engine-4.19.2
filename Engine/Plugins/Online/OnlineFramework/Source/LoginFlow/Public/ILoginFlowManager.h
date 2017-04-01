// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SharedPointer.h"

class IOnlineSubsystem;
class SWidget;

/**
 * Create and configure one of these to enable web login flow in your application
 *
 * OnlineSubsystemFacebook for Windows requires this
 */
class LOGINFLOW_API ILoginFlowManager
	: public TSharedFromThis<ILoginFlowManager>
{
public:
	ILoginFlowManager() {}
	virtual ~ILoginFlowManager() {}

	/**
	 * Called when a popup is ready to be dismissed.
	 */
	DECLARE_DELEGATE(FOnPopupDismissed);

	/**
	 * Fired when a login flow Pop-Up (web) window needs to be displayed.
	 *
	 * @Returns Delegate that will be called when the popup is dismissed (to clean up any slate borders created)
	 * @Param PopupWidget The widget that contains the browser window.
	 */
	DECLARE_DELEGATE_RetVal_OneParam(FOnPopupDismissed, FOnDisplayPopup, const TSharedRef<SWidget>& /*LoginWidget*/);

	/**
	 * Register an online subsystem with the login flow factory (call at startup)
	 * 
	 * @param InOnlineIdentifier online subsystem identifier requiring a login flow UI
	 * @param InPopupDelegate external delegate to receive widgets from the login flow
	 */
	virtual bool AddLoginFlow(FName OnlineIdentifier, const FOnDisplayPopup& InPopupDelegate) = 0;

	/** Cancel an active login flow */
	virtual void CancelLoginFlow() = 0;
};
