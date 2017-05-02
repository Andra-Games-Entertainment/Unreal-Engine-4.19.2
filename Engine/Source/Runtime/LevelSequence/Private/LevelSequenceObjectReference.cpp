// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceObjectReference.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneFwd.h"

FLevelSequenceObjectReference::FLevelSequenceObjectReference(UObject* InObject, UObject* InContext)
{
	if (InObject->IsA<AActor>() || InObject->IsA<UActorComponent>())
	{
		if (InContext->IsA<AActor>())
		{
			// When the context is an actor, we only use path name lookup within the actor since it's assumed the actor will be a spawnable
			// As such, a persistent identifier for a dynamically spawned actor is not tenable
			ObjectPath = InObject->GetPathName(InContext);
			ObjectId = FUniqueObjectGuid();
		}
		else
		{
			ObjectId = FLazyObjectPtr(InObject).GetUniqueID();
			ObjectPath = InObject->GetPathName(InContext);
		}
	}
	else if(InObject->GetOuter() && InObject->GetOuter()->IsA<UActorComponent>())
	{
		ObjectId = FLazyObjectPtr(InObject).GetUniqueID();
		ObjectPath = InObject->GetPathName(InContext);
	}
}

FLevelSequenceObjectReference::FLevelSequenceObjectReference(const FUniqueObjectGuid& InObjectId, const FString& InObjectPath)
	: ObjectId(InObjectId)
	, ObjectPath(InObjectPath)
{
}

UObject* ResolveByPath(UObject* InContext, const FString& InObjectPath)
{
	if (!InObjectPath.IsEmpty())
	{
		if (UObject* FoundObject = FindObject<UObject>(InContext, *InObjectPath, false))
		{
			return FoundObject;
		}

		if (UObject* FoundObject = FindObject<UObject>(ANY_PACKAGE, *InObjectPath, false))
		{
			return FoundObject;
		}
	}

	return nullptr;
}

UObject* FLevelSequenceObjectReference::Resolve(UObject* InContext) const
{
	if (ObjectId.IsValid() && InContext != nullptr)
	{
		int32 PIEInstanceID = InContext->GetOutermost()->PIEInstanceID;
		FUniqueObjectGuid FixedUpId = PIEInstanceID == -1 ? ObjectId : ObjectId.FixupForPIE(PIEInstanceID);

		if (PIEInstanceID != -1 && FixedUpId == ObjectId)
		{
			UObject* FoundObject = ResolveByPath(InContext, ObjectPath);
			if (FoundObject)
			{
				return FoundObject;
			}

			UE_LOG(LogMovieScene, Warning, TEXT("Attempted to resolve object with a PIE instance that has not been fixed up yet. This is probably due to a streamed level not being available yet."));
			return nullptr;
		}

		FLazyObjectPtr LazyPtr;
		LazyPtr = FixedUpId;

		if (UObject* FoundObject = LazyPtr.Get())
		{
			return FoundObject;
		}
	}

	return ResolveByPath(InContext, ObjectPath);
}

bool FLevelSequenceObjectReferenceMap::Serialize(FArchive& Ar)
{
	int32 Num = Map.Num();
	Ar << Num;

	if (Ar.IsLoading())
	{
		while(Num-- > 0)
		{
			FGuid Key;
			Ar << Key;

			FLevelSequenceObjectReference Value;
			Ar << Value;

			Map.Add(Key, Value);
		}
	}
	else if (Ar.IsSaving() || Ar.IsCountingMemory() || Ar.IsObjectReferenceCollector())
	{
		for (auto& Pair : Map)
		{
			Ar << Pair.Key;
			Ar << Pair.Value;
		}
	}
	return true;
}

bool FLevelSequenceObjectReferenceMap::HasBinding(const FGuid& ObjectId) const
{
	if (const FLevelSequenceObjectReference* Reference = Map.Find(ObjectId))
	{
		return Reference->IsValid();
	}
	return false;
}

void FLevelSequenceObjectReferenceMap::CreateBinding(const FGuid& ObjectId, UObject* InObject, UObject* InContext)
{
	FLevelSequenceObjectReference NewReference(InObject, InContext);

	if (ensureMsgf(NewReference.IsValid(), TEXT("Unable to generate a reference for the specified object and context")))
	{
		Map.FindOrAdd(ObjectId) = NewReference;
	}
}

void FLevelSequenceObjectReferenceMap::CreateBinding(const FGuid& ObjectId, const FLevelSequenceObjectReference& ObjectReference)
{
	if (ensureMsgf(ObjectReference.IsValid(), TEXT("Invalid object reference specifed for binding")))
	{
		Map.FindOrAdd(ObjectId) = ObjectReference;
	}
}

void FLevelSequenceObjectReferenceMap::RemoveBinding(const FGuid& ObjectId)
{
	Map.Remove(ObjectId);
}

UObject* FLevelSequenceObjectReferenceMap::ResolveBinding(const FGuid& ObjectId, UObject* InContext) const
{
	const FLevelSequenceObjectReference* Reference = Map.Find(ObjectId);
	UObject* ResolvedObject = Reference ? Reference->Resolve(InContext) : nullptr;
	if (ResolvedObject != nullptr)
	{
		// if the resolved object does not have a valid world (e.g. world is being torn down), dont resolve
		return ResolvedObject->GetWorld() != nullptr ? ResolvedObject : nullptr;
	}
	return nullptr;
}

FGuid FLevelSequenceObjectReferenceMap::FindBindingId(UObject* InObject, UObject* InContext) const
{
	for (const auto& Pair : Map)
	{
		if (Pair.Value.Resolve(InContext) == InObject)
		{
			return Pair.Key;
		}
	}
	return FGuid();
}
