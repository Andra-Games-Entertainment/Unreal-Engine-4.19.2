// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraEditorCommon.h"
#include "NiagaraNode.h"
#include "NiagaraNodeFunctionCall.generated.h"

class UNiagaraScript;

UCLASS(MinimalAPI)
class UNiagaraNodeFunctionCall : public UNiagaraNode
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "Function")
	UNiagaraScript* FunctionScript;


	//Begin UObject interface
	virtual void PostLoad()override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)override;
	//End UObject interface

	//~ Begin UNiagaraNode Interface
	virtual void Compile(class INiagaraCompiler* Compiler, TArray<FNiagaraNodeResult>& Outputs)override;
	//End UNiagaraNode interface

	//~ Begin EdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	//~ End EdGraphNode Interface
};

