// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ClothingAssetInterface.h"

#include "ClothingSystemRuntimeTypes.h"
#include "GPUSkinPublicDefs.h"
#include "SkeletalMeshTypes.h"

#include "ClothingAsset.generated.h"

class UClothingAsset;

namespace nvidia
{
	namespace apex
	{
		class ClothingAsset;
	}
}

DECLARE_LOG_CATEGORY_EXTERN(LogClothingAsset, Log, All);

// Bone data for a vertex
USTRUCT()
struct FClothVertBoneData
{
	GENERATED_BODY()

	// Up to MAX_TOTAL_INFLUENCES bone indices that this vert is weighted to
	UPROPERTY()
	uint16 BoneIndices[MAX_TOTAL_INFLUENCES];

	// The weights for each entry in BoneIndices
	UPROPERTY()
	float BoneWeights[MAX_TOTAL_INFLUENCES];
};

/**
 *	Physical mesh data created during asset import or created from a skeletal mesh
 */
USTRUCT()
struct FClothPhysicalMeshData
{
	GENERATED_BODY()

	// Positions of each simulation vertex
	UPROPERTY(EditAnywhere, Category = SimMesh)
	TArray<FVector> Vertices;

	// Normal at each vertex
	UPROPERTY(EditAnywhere, Category = SimMesh)
	TArray<FVector> Normals;

	// Indices of the simulation mesh triangles
	UPROPERTY(EditAnywhere, Category = SimMesh)
	TArray<uint32> Indices;

	// The distance that each vertex can move away from its reference (skinned) position
	UPROPERTY(EditAnywhere, Category = SimMesh)
	TArray<float> MaxDistances;

	// Distance along the plane of the surface that the particles can travel (separation constraint)
	UPROPERTY(EditAnywhere, Category = SimMesh)
	TArray<float> BackstopDistances;

	// Radius of movement to allow for backstop movement
	UPROPERTY(EditAnywhere, Category = SimMesh)
	TArray<float> BackstopRadiuses;

	// Inverse mass for each vertex in the physical mesh
	UPROPERTY(EditAnywhere, Category = SimMesh)
	TArray<float> InverseMasses;

	// Indices and weights for each vertex, used to skin the mesh to create the reference pose
	UPROPERTY(EditAnywhere, Category = SimMesh)
	TArray<FClothVertBoneData> BoneData;

	// Maximum number of bone weights of any vetex
	UPROPERTY(EditAnywhere, Category = SimMesh)
	int32 MaxBoneWeights;

	// Number of fixed verts in the simulation mesh (fixed verts are just skinned and do not simulate)
	UPROPERTY(EditAnywhere, Category = SimMesh)
	int32 NumFixedVerts;
};

USTRUCT()
struct FClothLODData
{
	GENERATED_BODY()

	// Raw phys mesh data
	UPROPERTY(EditAnywhere, Category = SimMesh)
	FClothPhysicalMeshData PhysicalMeshData;

	// Collision primitive and covex data for clothing collisions
	UPROPERTY(EditAnywhere, Category = Collision)
	FClothCollisionData CollisionData;

	// Skinning data for transitioning from a higher detail LOD to this one
	TArray<FMeshToMeshVertData> TransitionUpSkinData;

	// Skinning data for transitioning from a lower detail LOD to this one
	TArray<FMeshToMeshVertData> TransitionDownSkinData;

	bool Serialize(FArchive& Ar)
	{
		UScriptStruct* Struct = FClothLODData::StaticStruct();

		// Serialize normal tagged data
		if (!Ar.IsCountingMemory())
		{
			Struct->SerializeTaggedProperties(Ar, (uint8*)this, Struct, nullptr);
		}

		// Serialize the mesh to mesh data (not a USTRUCT)
		Ar	<< TransitionUpSkinData
			<< TransitionDownSkinData;

		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FClothLODData> : public TStructOpsTypeTraitsBase2<FClothLODData>
{
	enum
	{
		WithSerializer = true,
	};
};

// Container for a constraint setup, these can be horizontal, vertical, shear and bend
USTRUCT()
struct FClothConstraintSetup
{
	GENERATED_BODY()

	FClothConstraintSetup()
		: Stiffness(1.0f)
		, StiffnessMultiplier(1.0f)
		, StretchLimit(1.0f)
		, CompressionLimit(1.0f)
	{}

	// How stiff this constraint is, this affects how closely it will follow the desired position
	UPROPERTY(EditAnywhere, Category=Constraint)
	float Stiffness;

	// A multiplier affecting the above value
	UPROPERTY(EditAnywhere, Category = Constraint)
	float StiffnessMultiplier;

	// The hard limit on how far this constarint can stretch
	UPROPERTY(EditAnywhere, Category = Constraint)
	float StretchLimit;

	// The hard limit on how far this constraint can compress
	UPROPERTY(EditAnywhere, Category = Constraint)
	float CompressionLimit;
};

UENUM()
enum class EClothingWindMethod : uint8
{
	// Use legacy wind mode, where accelerations are modified directly by the simulation
	// with no regard for drag or lift
	Legacy,

	// Use updated wind calculation for NvCloth based solved taking into account
	// drag and lift, this will require those properties to be correctly set in
	// the clothing configuration
	Accurate
};

/** Holds initial, asset level config for clothing actors. */
USTRUCT()
struct FClothConfig
{
	GENERATED_BODY()

	FClothConfig()
		: WindMethod(EClothingWindMethod::Legacy)
		, SelfCollisionRadius(0.0f)
		, SelfCollisionStiffness(0.0f)
		, Damping(0.4f)
		, Friction(0.1f)
		, WindDragCoefficient(0.02f/100.0f)
		, WindLiftCoefficient(0.02f/100.0f)
		, LinearDrag(0.2f)
		, AngularDrag(0.2f)
		, LinearInertiaScale(1.0f)
		, AngularInertiaScale(1.0f)
		, CentrifugalInertiaScale(1.0f)
		, SolverFrequency(60.0f)
		, StiffnessFrequency(60.0f)
		, GravityScale(1.0f)
		, TetherStiffness(1.0f)
		, TetherLimit(1.0f)
		, CollisionThickness(1.0f)
	{}

	bool HasSelfCollision() const;

	// How wind should be processed, Accurate uses drag and lift to make the cloth react differently, legacy applies similar forces to all clothing without drag and lift (similar to APEX)
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	EClothingWindMethod WindMethod;

	// Constraint data for vertical constraints
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	FClothConstraintSetup VerticalConstraintConfig;

	// Constraint data for horizontal constraints
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	FClothConstraintSetup HorizontalConstraintConfig;

	// Constraint data for bend constraints
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	FClothConstraintSetup BendConstraintConfig;

	// Constraint data for shear constraints
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	FClothConstraintSetup ShearConstraintConfig;

	// Size of self collision spheres centered on each vert
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float SelfCollisionRadius;

	// Stiffness of the spring force that will resolve self collisions
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float SelfCollisionStiffness;

	// Damping of particle motion per-axis
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	FVector Damping;

	// Friction of the surface when colliding
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float Friction;

	// Drag coefficient for wind calculations, higher values mean wind has more lateral effect on cloth
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float WindDragCoefficient;

	// Lift coefficient for wind calculations, higher values make cloth rise easier in wind
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float WindLiftCoefficient;

	// Drag applied to linear particle movement per-axis
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	FVector LinearDrag;

	// Drag applied to angular particle movement, higher values should limit material bending (per-axis)
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	FVector AngularDrag;

	// Scale for linear particle inertia, how much movement should translate to linear motion (per-axis)
	UPROPERTY(EditAnywhere, Category = ClothConfig, meta = (UIMin="0", UIMax="1", ClampMin="0", ClampMax="1"))
	FVector LinearInertiaScale;

	// Scale for angular particle inertia, how much movement should translate to angular motion (per-axis)
	UPROPERTY(EditAnywhere, Category = ClothConfig, meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	FVector AngularInertiaScale;

	// Scale for centrifugal particle inertia, how much movement should translate to angular motion (per-axis)
	UPROPERTY(EditAnywhere, Category = ClothConfig, meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	FVector CentrifugalInertiaScale;

	// Frequency of the position solver, lower values will lead to stretchier, bouncier cloth
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float SolverFrequency;

	// Frequency for stiffness calculations, lower values will degrade stiffness of constraints
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float StiffnessFrequency;

	// Scale of gravity effect on particles
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float GravityScale;

	// Scale for stiffness of particle tethers between each other
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float TetherStiffness;

	// Scale for the limit of particle tethers (how far they can separate)
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float TetherLimit;

	// 'Thickness' of the simulated cloth, used to adjust collisions
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float CollisionThickness;
};

namespace ClothingAssetUtils
{
	// Helper struct to hold binding information on a clothing asset, used to enumerate all of the
	// bindings on a skeletal mesh with GetMeshClothingAssetBindings below.
	struct FClothingAssetMeshBinding
	{
		UClothingAsset* Asset;
		int32 LODIndex;
		int32 SectionIndex;
		int32 AssetInternalLodIndex;
	};

	/**
	 * Given a skeletal mesh, find all of the currently bound clothing assets and their binding information
	 * @param InSkelMesh - The skeletal mesh to search
	 * @param OutBindings - The list of bindings to write to
	 */
	void CLOTHINGSYSTEMRUNTIME_API GetMeshClothingAssetBindings(USkeletalMesh* InSkelMesh, TArray<FClothingAssetMeshBinding>& OutBindings);
	
	// Similar to above, but only inspects the specified LOD
	void CLOTHINGSYSTEMRUNTIME_API GetMeshClothingAssetBindings(USkeletalMesh* InSkelMesh, TArray<FClothingAssetMeshBinding>& OutBindings, int32 InLodIndex);

	/**
	 * Given mesh information for two meshes, generate a list of skinning data to embed mesh0 in mesh1
	 * @param OutSkinningData	- Final skinning data to map mesh0 to mesh1
	 * @param Mesh0Verts		- Vertex positions for Mesh0
	 * @param Mesh0Normals		- Vertex normals for Mesh0
	 * @param Mesh0Tangents		- Vertex tangents for Mesh0
	 * @param Mesh1Verts		- Vertex positions for Mesh1
	 * @param Mesh1Normals		- Vertex normals for Mesh1
	 * @param Mesh1Indices		- Triangle indices for Mesh1
	 */
	void GenerateMeshToMeshSkinningData(TArray<FMeshToMeshVertData>& OutSkinningData,
													 const TArray<FVector>& Mesh0Verts,
													 const TArray<FVector>& Mesh0Normals,
													 const TArray<FVector>& Mesh0Tangents,
													 const TArray<FVector>& Mesh1Verts,
													 const TArray<FVector>& Mesh1Normals,
													 const TArray<uint32>& Mesh1Indices);

	/**
	 * Given a triangle ABC with normals at each vertex NA, NB and NC, get a barycentric coordinate
	 * and corresponding distance from the triangle encoded in an FVector4 where the components are
	 * (BaryX, BaryY, BaryZ, Dist)
	 * @param A		- Position of triangle vertex A
	 * @param B		- Position of triangle vertex B
	 * @param C		- Position of triangle vertex C
	 * @param NA	- Normal at vertex A
	 * @param NB	- Normal at vertex B
	 * @param NC	- Normal at vertex C
	 * @param Point	- Point to calculate Bary+Dist for
	 */
	FVector4 GetPointBaryAndDist(const FVector& A,
								 const FVector& B,
								 const FVector& C,
								 const FVector& NA,
								 const FVector& NB,
								 const FVector& NC,
								 const FVector& Point);
}

/**
 * Custom data wrapper for clothing assets
 * If writing a new clothing asset importer, creating a new derived custom data is how to store importer (and possibly simulation)
 * data that importer will create. This needs to be set to the CustomData member on the asset your factory creates.
 * Testing whether a UClothingAsset was made from a custom plugin can be achieved with
 * if(AssetPtr->CustomData->IsA(UMyCustomData::StaticClass()))
 */
UCLASS(abstract, MinimalAPI)
class UClothingAssetCustomData : public UObject
{
	GENERATED_BODY()

public:
	virtual void BindToSkeletalMesh(USkeletalMesh* InSkelMesh, int32 InMeshLodIndex, int32 InSectionIndex, int32 InAssetLodIndex)
	{}
};

UCLASS(hidecategories = Object, BlueprintType)
class CLOTHINGSYSTEMRUNTIME_API UClothingAsset : public UClothingAssetBase
{
	GENERATED_BODY()

public:

	UClothingAsset(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	// UClothingAssetBase Interface ////////////////////////////////////////////
	virtual bool BindToSkeletalMesh(USkeletalMesh* InSkelMesh, int32 InMeshLodIndex, int32 InSectionIndex, int32 InAssetLodIndex) override;
	virtual void UnbindFromSkeletalMesh(USkeletalMesh* InSkelMesh) override;
	virtual void UnbindFromSkeletalMesh(USkeletalMesh* InSkelMesh, int32 InMeshLodIndex) override;
	virtual void RefreshBoneMapping(USkeletalMesh* InSkelMesh) override;
	virtual void InvalidateCachedData() override;
	// End UClothingAssetBase Interface ////////////////////////////////////////

	/**
	*	Builds the LOD transition data
	*	When we transition between LODs we skin the incoming mesh to the outgoing mesh
	*	in exactly the same way the render mesh is skinned to create a smooth swap
	*/
	void BuildLodTransitionData();
#endif

	// Configuration of the cloth, contains all the parameters for how the clothing behaves
	UPROPERTY(EditAnywhere, Category = Config)
	FClothConfig ClothConfig;

	// The actual asset data, listed by LOD
	UPROPERTY()
	TArray<FClothLODData> LodData;

	// Tracks which clothing LOD each skel mesh LOD corresponds to (LodMap[SkelLod]=ClothingLod)
	UPROPERTY()
	TArray<int32> LodMap;

	// List of bones this asset uses inside its parent mesh
	UPROPERTY()
	TArray<FName> UsedBoneNames;

	// List of the indices for the bones in UsedBoneNames, used for remapping
	UPROPERTY()
	TArray<int32> UsedBoneIndices;

	/** Custom data applied by the importer depending on where the asset was imported from */
	UPROPERTY()
	UClothingAssetCustomData* CustomData;

private:

};
