// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class FControlRigEditor;

struct FControlRigTabSummoner : public FWorkflowTabFactory
{
public:
	static const FName TabID;
	
public:
	FControlRigTabSummoner(const TSharedRef<FControlRigEditor>& InControlRigEditor);
	
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	
protected:
	TWeakPtr<FControlRigEditor> ControlRigEditor;
};
