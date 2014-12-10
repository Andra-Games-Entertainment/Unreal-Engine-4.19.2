// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


// forward declarations
enum class EMessageScope;
class IReceiveMessages;


/**
 * Interface for message subscriptions.
 *
 * This interface provides access to a message subscription that was previously created with the
 * @see IMessageBus.Subscribe method. It can be used to query the subscription's details using the
 * various getters and modify its enabled state using the @see IMessageSubscription.Enable and
 * IMessageSubscription.Disable methods.
 *
 * A subscription that is disabled will cause a message endpoint to stop receiving messages for
 * the subscribed message type. However, the subscription is merely disabled and not removed.
 */
class IMessageSubscription
{
public:

	/**
	 * Disables the subscription.
	 *
	 * @see Enable, IsEnabled
	 */
	virtual void Disable() = 0;

	/**
	 * Enables the subscription.
	 *
	 * @see Disable, IsEnabled
	 */
	virtual void Enable() = 0;

	/**
	 * Gets the type of subscribed messages.
	 *
	 * @return Message type.
	 * @see GetScopeRange, GetSubscriber
	 */
	virtual FName GetMessageType() = 0;

	/**
	 * Gets the range of subscribed message scopes.
	 *
	 * @return Message scope range.
	 * @see GetMessageType, GetSubscriber
	 */
	virtual const TRange<EMessageScope>& GetScopeRange() = 0;

	/**
	 * Gets the subscriber.
	 *
	 * @return The subscriber.
	 * @see GetMessageType, GetScopeRange
	 */
	virtual const TWeakPtr<IReceiveMessages, ESPMode::ThreadSafe>& GetSubscriber() = 0;

	/**
	 * Checks whether the subscription is enabled.
	 *
	 * @return true if the subscription is enabled, false otherwise.
	 * @see Enable, Disable
	 */
	virtual bool IsEnabled() = 0;

public:

	/** Virtual destructor. */
	virtual ~IMessageSubscription() { }
};


/** Type definition for shared pointers to instances of IMessageSubscription. */
typedef TSharedPtr<IMessageSubscription, ESPMode::ThreadSafe> IMessageSubscriptionPtr;

/** Type definition for shared references to instances of IMessageSubscription. */
typedef TSharedRef<IMessageSubscription, ESPMode::ThreadSafe> IMessageSubscriptionRef;
