// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNiagaraEditor, All, All);

/** Information about a Niagara operation. */
class FNiagaraOpInfo
{
public:
	FNiagaraOpInfo()
		: NumericOuputTypeSelectionMode(ENiagaraNumericOutputTypeSelectionMode::Largest)
	{}

	FName Name;
	FText Category;
	FText FriendlyName;
	FText Description;
	ENiagaraNumericOutputTypeSelectionMode NumericOuputTypeSelectionMode;
	TArray<FNiagaraOpInOutInfo> Inputs;
	TArray<FNiagaraOpInOutInfo> Outputs;

	static TMap<FName, int32> OpInfoMap;
	static TArray<FNiagaraOpInfo> OpInfos;

	static void Init();
	static const FNiagaraOpInfo* GetOpInfo(FName OpName);
	static const TArray<FNiagaraOpInfo>& GetOpInfoArray();

	void BuildName(FString InName, FString InCategory);
};

UENUM()
enum class ENiagaraDataSetAccessMode : uint8
{
	/** Data set reads and writes use shared counters to add and remove the end of available data. Writes are conditional and read */
	AppendConsume,
	/** Data set is accessed directly at a specific index. */
	Direct,

	Num UMETA(Hidden),
};
