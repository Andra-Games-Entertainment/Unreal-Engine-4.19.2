#pragma once

#include "CoreMinimal.h"

namespace MagicLeap
{
	/**
	* Provides an interface between the AppFramework and any system that needs to be
	* manually paused/resumed when the application is paused/resumed.
	*/
	class MAGICLEAP_API IAppEventHandler
	{
	public:
		/** Adds the IAppEventHandler instance to the application's list of IAppEventHandler instances.*/
		IAppEventHandler();

		/** Removes the IAppEventHandler instance from the application's list of IAppEventHandler instances.*/
		virtual ~IAppEventHandler();

		/**
			Can be overridden by inheriting class that needs to perform certain initializations after the app is finished
			booting up.  AppFramework remains agnostic of the subsystem being initialized.
		*/
		virtual void OnAppStartup() {}

		virtual void OnAppShutDown() {}

		/**
			Must be overridden by inheriting class in order to pause its system.  AppFramework
			remains agnostic of the subsystem being paused.
		*/
		virtual void OnAppPause() = 0;

		/**
			Must be overridden by inheriting class in order to resume its system.  AppFramework
			remains agnostic of the subsystem being resumed.
		*/
		virtual void OnAppResume() = 0;

	protected:
		bool bWasSystemEnabledOnPause;
	};
} // MagicLeap
