#pragma once

#include "NiagaraEvents.h"
#include "WorldCollision.h"

UENUM()
enum class ENiagaraCollisionMode : uint8
{
	None = 0,
	SceneGeometry,
	DepthBuffer,
	DistanceField
};


struct FNiagaraCollisionTrace
{
	FTraceHandle CollisionTraceHandle;
	uint32 SourceParticleIndex;
	FVector OriginalVelocity;
};


class FNiagaraCollisionBatch
{
public:
	FNiagaraCollisionBatch()
		: CollisionEventDataSet(nullptr)
		, EmitterName(NAME_None)
	{
	}

	~FNiagaraCollisionBatch()
	{
		FNiagaraEventDataSetMgr::Reset(OwnerEffectInstanceName, EmitterName);
	}

	void Tick()
	{
		if (CollisionEventDataSet)
		{
			CollisionEventDataSet->Tick();
		}
	}

	void Reset()
	{
		if (CollisionEventDataSet)
		{
			CollisionEventDataSet->SetNumInstances(0);
		}
	}

	void Init(FName InOwnerEffectInstanceName, FName InEmitterName)
	{
		if (CollisionEventDataSet)
		{
			CollisionEventDataSet->Reset();
		}
		EmitterName = InEmitterName;
		OwnerEffectInstanceName = InOwnerEffectInstanceName;
		CollisionEventDataSet = FNiagaraEventDataSetMgr::CreateEventDataSet(OwnerEffectInstanceName, EmitterName, NIAGARA_BUILTIN_EVENTNAME_COLLISION);
		// TODO: this should go away once we can use the FNiagaraCollisionEventPayload
		// UStruct to create the data set
		//FNiagaraVariable ValidVar(FNiagaraTypeDefinition::GetIntDef(), "Valid");
		FNiagaraVariable PosVar(FNiagaraTypeDefinition::GetVec3Def(), "CollisionLocation");
		FNiagaraVariable NrmVar(FNiagaraTypeDefinition::GetVec3Def(), "CollisionNormal");
		FNiagaraVariable PhysMatIdxVar(FNiagaraTypeDefinition::GetIntDef(), "PhysicalMaterialIndex");
		FNiagaraVariable VelVar(FNiagaraTypeDefinition::GetVec3Def(), "CollisionVelocity");
		FNiagaraVariable ParticleIndexVar(FNiagaraTypeDefinition::GetIntDef(), "ParticleIndex");
		//CollisionEventDataSet->AddVariable(ValidVar);
		CollisionEventDataSet->AddVariable(PosVar);
		CollisionEventDataSet->AddVariable(NrmVar);
		CollisionEventDataSet->AddVariable(PhysMatIdxVar);
		CollisionEventDataSet->AddVariable(VelVar);
		CollisionEventDataSet->AddVariable(ParticleIndexVar);
		CollisionEventDataSet->Finalize();
	}
	
	void KickoffNewBatch(struct FNiagaraSimulation *Sim, float DeltaSeconds);

	void GenerateEventsFromResults(struct FNiagaraSimulation *Sim);

	const FNiagaraDataSet *GetDataSet() const { return CollisionEventDataSet; }
private:
	TArray<FTraceHandle> CollisionTraceHandles;
	TArray<FNiagaraCollisionTrace> CollisionTraces;
	TArray<FNiagaraCollisionEventPayload> CollisionEvents;
	FNiagaraDataSet *CollisionEventDataSet;
	FName EmitterName;
	FName OwnerEffectInstanceName;
};