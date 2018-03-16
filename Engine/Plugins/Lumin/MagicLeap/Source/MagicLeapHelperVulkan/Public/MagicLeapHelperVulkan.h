// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "RenderingThread.h"


struct FRHICommandExecute_Void final : public FRHICommand<FRHICommandExecute_Void>
{
	TFunction<void()> Function;

	FRHICommandExecute_Void(const TFunction<void()>& InFunction)
		: Function(InFunction)
	{
	}

	void Execute(FRHICommandListBase& RHICmdList)
	{
		Function();
	}
};

static void ExecuteOnRHIThread_DoNotWait(const TFunction<void()>& Function)
{
#if DO_CHECK
	check(IsInRenderingThread() || IsInRHIThread());
#endif

	FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();

	if (GRHIThreadId && !RHICmdList.Bypass())
	{
		new (RHICmdList.AllocCommand<FRHICommandExecute_Void>()) FRHICommandExecute_Void(Function);
	}
	else
	{
		Function();
	}
}

class MAGICLEAPHELPERVULKAN_API FMagicLeapHelperVulkan
{
public:
	static void BlitImage(uint64 SrcName, int32 SrcLevel, int32 SrcX, int32 SrcY, int32 SrcZ, int32 SrcWidth, int32 SrcHeight, int32 SrcDepth, uint64 DstName, int32 DstLevel, int32 DstX, int32 DstY, int32 DstZ, int32 DstWidth, int32 DstHeight, int32 DstDepth);
	static void SignalObjects(uint64 SignalObject0, uint64 SignalObject1);
	static void TestClear(uint64 Dest);
};
