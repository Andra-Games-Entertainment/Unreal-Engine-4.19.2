// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "OnlineMessageMultiTaskOculus.h"
#include "OnlineSubsystemOculusPrivate.h"

void FOnlineMessageMultiTaskOculus::AddNewRequest(ovrRequest RequestId)
{
	OculusSubsystem.AddRequestDelegate(
		RequestId,
		FOculusMessageOnCompleteDelegate::CreateLambda([this, RequestId](ovrMessageHandle Message, bool bIsError)
	{
		InProgressRequests.Remove(RequestId);
		if (bIsError)
		{
			bDidAllRequestsFinishedSuccessfully = false;
		}

		if (InProgressRequests.Num() == 0)
		{
			Delegate.ExecuteIfBound();
		}
	}));
}
