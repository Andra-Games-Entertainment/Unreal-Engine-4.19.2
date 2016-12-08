// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PackageArchiveProxy.h"
#include "UnrealHeaderTool.h"
#include "UObject/Package.h"
#include "Templates/Casts.h"

FPackageArchiveProxy::FPackageArchiveProxy(FUHTMakefile& UHTMakefile, UPackage* Package)
	: FObjectBaseArchiveProxy(UHTMakefile, Package)
{
	PackageFlags = Package->GetPackageFlags();
	MetadataArchiveProxy = FMetadataArchiveProxy(UHTMakefile, Package->GetMetaData());
}

FArchive& operator<<(FArchive& Ar, FPackageArchiveProxy& PackageArchiveProxy)
{
	Ar << static_cast<FObjectBaseArchiveProxy&>(PackageArchiveProxy);
	Ar << PackageArchiveProxy.PackageFlags;
	Ar << PackageArchiveProxy.MetadataArchiveProxy;

	return Ar;
}

void FPackageArchiveProxy::AddReferencedNames(UPackage* Package, FUHTMakefile& UHTMakefile)
{
	FObjectBaseArchiveProxy::AddReferencedNames(Package, UHTMakefile);
	FMetadataArchiveProxy::AddReferencedNames(Package->GetMetaData(), UHTMakefile);
}

void FPackageArchiveProxy::Resolve(UPackage* Package, const FUHTMakefile& UHTMakefile) const
{
	FObjectBaseArchiveProxy::Resolve(Package, UHTMakefile);
	MetadataArchiveProxy.Resolve(Package->GetMetaData(), UHTMakefile);
}

UPackage* FPackageArchiveProxy::CreatePackage(const FUHTMakefile& UHTMakefile)
{
	FName PackageName = Name.CreateName(UHTMakefile);
	UPackage* Package = Cast<UPackage>(StaticFindObjectFast(UPackage::StaticClass(), nullptr, PackageName, false, false));
	if (!Package)
	{
		Package = ::CreatePackage(nullptr, *PackageName.ToString());
	}
	PostConstruct(Package);
	return Package;
}

void FPackageArchiveProxy::PostConstruct(UPackage* Package) const
{
	FObjectBaseArchiveProxy::PostConstruct(Package);
	Package->SetPackageFlagsTo(PackageFlags);
}
