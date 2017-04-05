// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "USDAssetImportFactory.h"
#include "USDImporter.h"
#include "IUSDImporterModule.h"
#include "ActorFactories/ActorFactoryStaticMesh.h"
#include "ScopedTimers.h"
#include "USDImportOptions.h"
#include "Engine/StaticMesh.h"
#include "Paths.h"

void FUSDAssetImportContext::Init(UObject* InParent, const FString& InName, EObjectFlags InFlags, class IUsdStage* InStage)
{
	FUsdImportContext::Init(InParent, InName, InFlags, InStage);
}


UUSDAssetImportFactory::UUSDAssetImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = false;
	bEditAfterNew = true;
	SupportedClass = UStaticMesh::StaticClass();

	bEditorImport = true;
	bText = false;

	Formats.Add(TEXT("usd;Universal Scene Descriptor files"));
	Formats.Add(TEXT("usda;Universal Scene Descriptor files"));
	Formats.Add(TEXT("usdc;Universal Scene Descriptor files"));
}

UObject* UUSDAssetImportFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	UObject* ImportedObject = nullptr;

	UUSDImportOptions* ImportOptions = NewObject<UUSDImportOptions>(this);

	UUSDImporter* USDImporter = IUSDImporterModule::Get().GetImporter();

	if (USDImporter->ShowImportOptions(*ImportOptions))
	{
		IUsdStage* Stage = USDImporter->ReadUSDFile(Filename);
		if (Stage)
		{
			ImportContext.Init(InParent, InName.ToString(), Flags, Stage);
			ImportContext.ImportOptions = ImportOptions;
			ImportContext.bApplyWorldTransformToGeometry = ImportOptions->bApplyWorldTransformToGeometry;

			TArray<FUsdPrimToImport> PrimsToImport;
			USDImporter->FindPrimsToImport(ImportContext, PrimsToImport);

			ImportedObject = USDImporter->ImportMeshes(ImportContext, PrimsToImport);

			// Just return the first one imported
			ImportedObject = ImportContext.PrimToAssetMap.Num() > 0 ? ImportContext.PrimToAssetMap.CreateConstIterator().Value() : nullptr;
		}
	}

	ImportContext.DisplayErrorMessages();

	return ImportedObject;
}

bool UUSDAssetImportFactory::FactoryCanImport(const FString& Filename)
{
	const FString Extension = FPaths::GetExtension(Filename);

	if (Extension == TEXT("usd") || Extension == TEXT("usda") || Extension == TEXT("usdc"))
	{
		return true;
	}

	return false;
}

void UUSDAssetImportFactory::CleanUp()
{
	ImportContext = FUSDAssetImportContext();
	UnrealUSDWrapper::CleanUp();
}
