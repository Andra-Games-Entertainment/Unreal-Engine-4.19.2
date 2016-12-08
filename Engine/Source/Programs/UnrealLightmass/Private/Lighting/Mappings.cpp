// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Mappings.h"
#include "Importer.h"
#include "StaticMesh.h"

namespace Lightmass
{
/** A static indicating that debug borders should be used around padded mappings. */
bool FStaticLightingMapping::s_bShowLightmapBorders = false;

void FStaticLightingMapping::Import( class FLightmassImporter& Importer )
{
	Importer.ImportData( (FStaticLightingMappingData*) this );
	Mesh = Importer.GetStaticMeshInstances().FindRef(Guid);
}

void FStaticLightingTextureMapping::Import( class FLightmassImporter& Importer )
{
	FStaticLightingMapping::Import( Importer );
	Importer.ImportData( (FStaticLightingTextureMappingData*) this );
	CachedSizeX = SizeX;
	CachedSizeY = SizeY;
	IrradiancePhotonCacheSizeX = 0;
	IrradiancePhotonCacheSizeY = 0;
}

} //namespace Lightmass
