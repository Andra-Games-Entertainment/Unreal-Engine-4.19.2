// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanPendingState.cpp: Private VulkanPendingState function definitions.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPendingState.h"
#include "VulkanPipeline.h"
#include "VulkanContext.h"

// RTs 0-3
#define NUMBITS_BLEND_STATE				4 //(x4=16) = 16 ++
#define NUMBITS_RENDER_TARGET_FORMAT	4 //(x4=16) = 32 ++
#define NUMBITS_LOAD_OP					2 //(x4=8) = 40 ++
#define NUMBITS_STORE_OP				2 //(x4=8) = 48 ++
#define NUMBITS_CULL_MODE				2 //(x1=2) = 50 ++
#define NUMBITS_POLYFILL				1 //(x1=1) = 51 ++
#define NUMBITS_POLYTYPE				3 //(x3=3) = 54 ++
#define NUMBITS_DEPTH_BIAS_ENABLED		1 //(x1=1) = 55 ++
#define NUMBITS_DEPTH_TEST_ENABLED		1 //(x1=1) = 56 ++
#define NUMBITS_DEPTH_WRITE_ENABLED		1 //(x1=1) = 57 ++
#define NUMBITS_DEPTH_COMPARE_OP		3 //(x1=3) = 60 ++
#define NUMBITS_FRONT_STENCIL_OP		4 //(x1=4) = 64 ++

// RTs 4-7
//#define NUMBITS_BLEND_STATE			4 //(x4=16) = 16 ++
//#define NUMBITS_RENDER_TARGET_FORMAT	3 //(x4=16) = 32 ++
//#define NUMBITS_LOAD_OP				2 //(x4=8) = 40 ++
//#define NUMBITS_STORE_OP				2 //(x4=8) = 48 ++
#define NUMBITS_BACK_STENCIL_OP			4 //(x1=4) = 52 ++
#define NUMBITS_STENCIL_TEST_ENABLED	1 //(x1=1) = 53 ++
#define NUMBITS_MSAA_ENABLED			1 //(x1=1) = 54 ++
#define NUMBITS_NUM_COLOR_BLENDS		3 //(x1=3) = 57 ++



#define OFFSET_BLEND_STATE0				(0)
#define OFFSET_BLEND_STATE1				(OFFSET_BLEND_STATE0			+ NUMBITS_BLEND_STATE)
#define OFFSET_BLEND_STATE2				(OFFSET_BLEND_STATE1			+ NUMBITS_BLEND_STATE)
#define OFFSET_BLEND_STATE3				(OFFSET_BLEND_STATE2			+ NUMBITS_BLEND_STATE)
#define OFFSET_RENDER_TARGET_FORMAT0	(OFFSET_BLEND_STATE3			+ NUMBITS_BLEND_STATE)
#define OFFSET_RENDER_TARGET_FORMAT1	(OFFSET_RENDER_TARGET_FORMAT0	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_RENDER_TARGET_FORMAT2	(OFFSET_RENDER_TARGET_FORMAT1	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_RENDER_TARGET_FORMAT3	(OFFSET_RENDER_TARGET_FORMAT2	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_RENDER_TARGET_LOAD0		(OFFSET_RENDER_TARGET_FORMAT3	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_RENDER_TARGET_LOAD1		(OFFSET_RENDER_TARGET_LOAD0		+ NUMBITS_LOAD_OP)
#define OFFSET_RENDER_TARGET_LOAD2		(OFFSET_RENDER_TARGET_LOAD1		+ NUMBITS_LOAD_OP)
#define OFFSET_RENDER_TARGET_LOAD3		(OFFSET_RENDER_TARGET_LOAD2		+ NUMBITS_LOAD_OP)
#define OFFSET_RENDER_TARGET_STORE0		(OFFSET_RENDER_TARGET_LOAD3		+ NUMBITS_LOAD_OP)
#define OFFSET_RENDER_TARGET_STORE1		(OFFSET_RENDER_TARGET_STORE0	+ NUMBITS_STORE_OP)
#define OFFSET_RENDER_TARGET_STORE2		(OFFSET_RENDER_TARGET_STORE1	+ NUMBITS_STORE_OP)
#define OFFSET_RENDER_TARGET_STORE3		(OFFSET_RENDER_TARGET_STORE2	+ NUMBITS_STORE_OP)
#define OFFSET_CULL_MODE				(OFFSET_RENDER_TARGET_STORE3	+ NUMBITS_STORE_OP)
#define OFFSET_POLYFILL					(OFFSET_CULL_MODE				+ NUMBITS_CULL_MODE)
#define OFFSET_POLYTYPE					(OFFSET_POLYFILL				+ NUMBITS_POLYFILL)
#define OFFSET_DEPTH_BIAS_ENABLED		(OFFSET_POLYTYPE				+ NUMBITS_POLYTYPE)
#define OFFSET_DEPTH_TEST_ENABLED		(OFFSET_DEPTH_BIAS_ENABLED		+ NUMBITS_DEPTH_BIAS_ENABLED)
#define OFFSET_DEPTH_WRITE_ENABLED		(OFFSET_DEPTH_TEST_ENABLED		+ NUMBITS_DEPTH_TEST_ENABLED)
#define OFFSET_DEPTH_COMPARE_OP			(OFFSET_DEPTH_WRITE_ENABLED		+ NUMBITS_DEPTH_WRITE_ENABLED)
#define OFFSET_FRONT_STENCIL_OP			(OFFSET_DEPTH_COMPARE_OP		+ NUMBITS_DEPTH_COMPARE_OP)
static_assert(OFFSET_FRONT_STENCIL_OP + NUMBITS_FRONT_STENCIL_OP <= 64, "Out of bits!");

#define OFFSET_BLEND_STATE4				(0x8000)
#define OFFSET_BLEND_STATE5				(OFFSET_BLEND_STATE4			+ NUMBITS_BLEND_STATE)
#define OFFSET_BLEND_STATE6				(OFFSET_BLEND_STATE5			+ NUMBITS_BLEND_STATE)
#define OFFSET_BLEND_STATE7				(OFFSET_BLEND_STATE6			+ NUMBITS_BLEND_STATE)
#define OFFSET_RENDER_TARGET_FORMAT4	(OFFSET_BLEND_STATE7			+ NUMBITS_BLEND_STATE)
#define OFFSET_RENDER_TARGET_FORMAT5	(OFFSET_RENDER_TARGET_FORMAT4	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_RENDER_TARGET_FORMAT6	(OFFSET_RENDER_TARGET_FORMAT5	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_RENDER_TARGET_FORMAT7	(OFFSET_RENDER_TARGET_FORMAT6	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_RENDER_TARGET_LOAD4		(OFFSET_RENDER_TARGET_FORMAT7	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_RENDER_TARGET_LOAD5		(OFFSET_RENDER_TARGET_LOAD4		+ NUMBITS_LOAD_OP)
#define OFFSET_RENDER_TARGET_LOAD6		(OFFSET_RENDER_TARGET_LOAD5		+ NUMBITS_LOAD_OP)
#define OFFSET_RENDER_TARGET_LOAD7		(OFFSET_RENDER_TARGET_LOAD6		+ NUMBITS_LOAD_OP)
#define OFFSET_RENDER_TARGET_STORE4		(OFFSET_RENDER_TARGET_LOAD7		+ NUMBITS_STORE_OP)
#define OFFSET_RENDER_TARGET_STORE5		(OFFSET_RENDER_TARGET_STORE4	+ NUMBITS_STORE_OP)
#define OFFSET_RENDER_TARGET_STORE6		(OFFSET_RENDER_TARGET_STORE5	+ NUMBITS_STORE_OP)
#define OFFSET_RENDER_TARGET_STORE7		(OFFSET_RENDER_TARGET_STORE6	+ NUMBITS_STORE_OP)
#define OFFSET_BACK_STENCIL_OP			(OFFSET_RENDER_TARGET_STORE7	+ NUMBITS_STORE_OP)
#define OFFSET_STENCIL_TEST_ENABLED		(OFFSET_BACK_STENCIL_OP			+ NUMBITS_BACK_STENCIL_OP)
#define OFFSET_MSAA_ENABLED				(OFFSET_STENCIL_TEST_ENABLED	+ NUMBITS_STENCIL_TEST_ENABLED)
#define OFFSET_NUM_COLOR_BLENDS			(OFFSET_MSAA_ENABLED	+ NUMBITS_MSAA_ENABLED)
static_assert(((OFFSET_NUM_COLOR_BLENDS + NUMBITS_NUM_COLOR_BLENDS) & ~0x8000) <= 64, "Out of bits!");

static const uint32 BlendBitOffsets[MaxSimultaneousRenderTargets] =
{
	OFFSET_BLEND_STATE0,
	OFFSET_BLEND_STATE1,
	OFFSET_BLEND_STATE2,
	OFFSET_BLEND_STATE3,
	OFFSET_BLEND_STATE4,
	OFFSET_BLEND_STATE5,
	OFFSET_BLEND_STATE6,
	OFFSET_BLEND_STATE7
};
static const uint32 RTFormatBitOffsets[MaxSimultaneousRenderTargets] =
{
	OFFSET_RENDER_TARGET_FORMAT0,
	OFFSET_RENDER_TARGET_FORMAT1,
	OFFSET_RENDER_TARGET_FORMAT2,
	OFFSET_RENDER_TARGET_FORMAT3,
	OFFSET_RENDER_TARGET_FORMAT4,
	OFFSET_RENDER_TARGET_FORMAT5,
	OFFSET_RENDER_TARGET_FORMAT6,
	OFFSET_RENDER_TARGET_FORMAT7
};
static const uint32 RTLoadBitOffsets[MaxSimultaneousRenderTargets] =
{
	OFFSET_RENDER_TARGET_LOAD0,
	OFFSET_RENDER_TARGET_LOAD1,
	OFFSET_RENDER_TARGET_LOAD2,
	OFFSET_RENDER_TARGET_LOAD3,
	OFFSET_RENDER_TARGET_LOAD4,
	OFFSET_RENDER_TARGET_LOAD5,
	OFFSET_RENDER_TARGET_LOAD6,
	OFFSET_RENDER_TARGET_LOAD7
};
static const uint32 RTStoreBitOffsets[MaxSimultaneousRenderTargets] =
{
	OFFSET_RENDER_TARGET_STORE0,
	OFFSET_RENDER_TARGET_STORE1,
	OFFSET_RENDER_TARGET_STORE2,
	OFFSET_RENDER_TARGET_STORE3,
	OFFSET_RENDER_TARGET_STORE4,
	OFFSET_RENDER_TARGET_STORE5,
	OFFSET_RENDER_TARGET_STORE6,
	OFFSET_RENDER_TARGET_STORE7
};

static FORCEINLINE uint64* GetKey(FVulkanPipelineGraphicsKey& Key, uint64 Offset)
{
	return Key.Key + (((Offset & 0x8000) != 0) ? 1 : 0);
}

static FORCEINLINE void SetKeyBits(FVulkanPipelineGraphicsKey& Key, uint64 Offset, uint64 NumBits, uint64 Value)
{
	uint64& CurrentKey = *GetKey(Key, Offset);
	Offset = (Offset & ~0x8000);
	const uint64 BitMask = ((1ULL << NumBits) - 1) << Offset;
	CurrentKey = (CurrentKey & ~BitMask) | (((uint64)(Value) << Offset) & BitMask); \
}

struct FDebugPipelineKey
{
	union
	{
		struct
		{
			uint64 BlendState0			: NUMBITS_BLEND_STATE;
			uint64 BlendState1			: NUMBITS_BLEND_STATE;
			uint64 BlendState2			: NUMBITS_BLEND_STATE;
			uint64 BlendState3			: NUMBITS_BLEND_STATE;
			uint64 RenderTargetFormat0	: NUMBITS_RENDER_TARGET_FORMAT;
			uint64 RenderTargetFormat1	: NUMBITS_RENDER_TARGET_FORMAT;
			uint64 RenderTargetFormat2	: NUMBITS_RENDER_TARGET_FORMAT;
			uint64 RenderTargetFormat3	: NUMBITS_RENDER_TARGET_FORMAT;
			uint64 RenderTargetLoad0	: NUMBITS_LOAD_OP;
			uint64 RenderTargetLoad1	: NUMBITS_LOAD_OP;
			uint64 RenderTargetLoad2	: NUMBITS_LOAD_OP;
			uint64 RenderTargetLoad3	: NUMBITS_LOAD_OP;
			uint64 RenderTargetStore0	: NUMBITS_STORE_OP;
			uint64 RenderTargetStore1	: NUMBITS_STORE_OP;
			uint64 RenderTargetStore2	: NUMBITS_STORE_OP;
			uint64 RenderTargetStore3	: NUMBITS_STORE_OP;
			uint64 CullMode				: NUMBITS_CULL_MODE;
			uint64 PolyFill				: NUMBITS_POLYFILL;
			uint64 PolyType				: NUMBITS_POLYTYPE;
			uint64 DepthBiasEnabled		: NUMBITS_DEPTH_BIAS_ENABLED;
			uint64 DepthTestEnabled		: NUMBITS_DEPTH_TEST_ENABLED;
			uint64 DepthWriteEnabled	: NUMBITS_DEPTH_WRITE_ENABLED;
			uint64 DepthCompareOp		: NUMBITS_DEPTH_COMPARE_OP;
			uint64 FrontStencilOp		: NUMBITS_FRONT_STENCIL_OP;
			uint64						: 0;

			uint64 BlendState4			: NUMBITS_BLEND_STATE;
			uint64 BlendState5			: NUMBITS_BLEND_STATE;
			uint64 BlendState6			: NUMBITS_BLEND_STATE;
			uint64 BlendState7			: NUMBITS_BLEND_STATE;
			uint64 RenderTargetFormat4	: NUMBITS_RENDER_TARGET_FORMAT;
			uint64 RenderTargetFormat5	: NUMBITS_RENDER_TARGET_FORMAT;
			uint64 RenderTargetFormat6	: NUMBITS_RENDER_TARGET_FORMAT;
			uint64 RenderTargetFormat7	: NUMBITS_RENDER_TARGET_FORMAT;
			uint64 RenderTargetLoad4	: NUMBITS_LOAD_OP;
			uint64 RenderTargetLoad5	: NUMBITS_LOAD_OP;
			uint64 RenderTargetLoad6	: NUMBITS_LOAD_OP;
			uint64 RenderTargetLoad7	: NUMBITS_LOAD_OP;
			uint64 RenderTargetStore4	: NUMBITS_STORE_OP;
			uint64 RenderTargetStore5	: NUMBITS_STORE_OP;
			uint64 RenderTargetStore6	: NUMBITS_STORE_OP;
			uint64 RenderTargetStore7	: NUMBITS_STORE_OP;
			uint64 BackStencilOp		: NUMBITS_BACK_STENCIL_OP;
			uint64 StencilTestEnabled	: NUMBITS_STENCIL_TEST_ENABLED;
			uint64 MSAAEnabled			: NUMBITS_MSAA_ENABLED;
			uint64 NumColorBlends		: NUMBITS_NUM_COLOR_BLENDS;
			uint64: 0;
		};
		uint64 Key[2];
	};

	FDebugPipelineKey()
	{
		static_assert(sizeof(*this) == sizeof(FVulkanPipelineGraphicsKey), "size mismatch!");
		{
			// Sanity check that bits match
			FVulkanPipelineGraphicsKey CurrentKeys;
			FMemory::Memzero(*this);

			SetKeyBits(CurrentKeys, OFFSET_CULL_MODE, NUMBITS_CULL_MODE, 1);
			CullMode = 1;

			SetKeyBits(CurrentKeys, OFFSET_BLEND_STATE0, NUMBITS_BLEND_STATE, 7);
			BlendState0 = 7;

			SetKeyBits(CurrentKeys, OFFSET_BLEND_STATE5, NUMBITS_BLEND_STATE, 3);
			BlendState5 = 3;

			SetKeyBits(CurrentKeys, OFFSET_MSAA_ENABLED, NUMBITS_MSAA_ENABLED, 1);
			MSAAEnabled = 1;

			check(CurrentKeys.Key[0] == Key[0] && CurrentKeys.Key[1] == Key[1]);
		}

		{
			FVulkanPipelineGraphicsKey CurrentKeys;
			FMemory::Memzero(*this);

			SetKeyBits(CurrentKeys, OFFSET_POLYFILL, NUMBITS_POLYFILL, 1);
			PolyFill = 1;

			SetKeyBits(CurrentKeys, OFFSET_RENDER_TARGET_LOAD2, NUMBITS_LOAD_OP, 2);
			RenderTargetLoad2 = 2;

			SetKeyBits(CurrentKeys, OFFSET_BLEND_STATE5, NUMBITS_BLEND_STATE, 3);
			BlendState5 = 3;

			SetKeyBits(CurrentKeys, OFFSET_FRONT_STENCIL_OP, NUMBITS_FRONT_STENCIL_OP, 5);
			FrontStencilOp = 5;

			SetKeyBits(CurrentKeys, OFFSET_NUM_COLOR_BLENDS, NUMBITS_NUM_COLOR_BLENDS, 2);
			NumColorBlends = 2;

			check(CurrentKeys.Key[0] == Key[0] && CurrentKeys.Key[1] == Key[1]);
		}
	}
};
static FDebugPipelineKey GDebugPipelineKey;
static_assert(sizeof(GDebugPipelineKey) == 2 * sizeof(uint64), "Debug struct not matching Hash/Sizes!");

FVulkanPendingState::FVulkanPendingState(FVulkanDevice* InDevice)
	: Device(InDevice)
	, GlobalUniformPool(nullptr)
{
	// Create the global uniform pool
	GlobalUniformPool = new FVulkanGlobalUniformPool();
}

FVulkanPendingState::~FVulkanPendingState()
{
	delete GlobalUniformPool;
	GlobalUniformPool = nullptr;
}

FVulkanPendingGfxState::FVulkanPendingGfxState(FVulkanDevice* InDevice)
	: FVulkanPendingState(InDevice)
	, bScissorEnable(false)
{
	Reset();

    // Create the global uniform pool
    GlobalUniformPool = new FVulkanGlobalUniformPool();
}

FVulkanPendingGfxState::~FVulkanPendingGfxState()
{
}


// Expected to be called after render pass has been ended
// and only from "FVulkanDynamicRHI::RHIEndDrawingViewport()"
void FVulkanPendingGfxState::Reset()
{
	CurrentState.Reset();

	FMemory::Memzero(PendingStreams);
}

FVulkanDescriptorPool::FVulkanDescriptorPool(FVulkanDevice* InDevice)
	: Device(InDevice)
	, DescriptorPool(VK_NULL_HANDLE)
	, MaxDescriptorSets(0)
	, NumAllocatedDescriptorSets(0)
	, PeakAllocatedDescriptorSets(0)
{
	// Increased from 8192 to prevent Protostar crashing on Mali
	MaxDescriptorSets = 16384;

	const VkPhysicalDeviceLimits& Limits = Device->GetLimits();
	FMemory::Memzero(MaxAllocatedTypes);
	FMemory::Memzero(NumAllocatedTypes);
	FMemory::Memzero(PeakAllocatedTypes);

	//#todo-rco: Get some initial values
	uint32 LimitMaxUniformBuffers = 2048;
	uint32 LimitMaxSamplers = 1024;
	uint32 LimitMaxCombinedImageSamplers = 4096;
	uint32 LimitMaxUniformTexelBuffers = 512;

	TArray<VkDescriptorPoolSize> Types;
	VkDescriptorPoolSize* Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	Type->descriptorCount = LimitMaxUniformBuffers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	Type->descriptorCount = LimitMaxUniformBuffers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_SAMPLER;
	Type->descriptorCount = LimitMaxSamplers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	Type->descriptorCount = LimitMaxCombinedImageSamplers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
	Type->descriptorCount = LimitMaxUniformTexelBuffers;

	for (const VkDescriptorPoolSize& PoolSize : Types)
	{
		MaxAllocatedTypes[PoolSize.type] = PoolSize.descriptorCount;
	}

	VkDescriptorPoolCreateInfo PoolInfo;
	FMemory::Memzero(PoolInfo);
	PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	PoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	PoolInfo.poolSizeCount = Types.Num();
	PoolInfo.pPoolSizes = Types.GetData();
	PoolInfo.maxSets = MaxDescriptorSets;

	VERIFYVULKANRESULT(VulkanRHI::vkCreateDescriptorPool(Device->GetInstanceHandle(), &PoolInfo, nullptr, &DescriptorPool));
}

FVulkanDescriptorPool::~FVulkanDescriptorPool()
{
	if (DescriptorPool != VK_NULL_HANDLE)
	{
		VulkanRHI::vkDestroyDescriptorPool(Device->GetInstanceHandle(), DescriptorPool, nullptr);
		DescriptorPool = VK_NULL_HANDLE;
	}
}

void FVulkanDescriptorPool::TrackAddUsage(const FVulkanDescriptorSetsLayout& Layout)
{
	// Check and increment our current type usage
	for (uint32 TypeIndex = VK_DESCRIPTOR_TYPE_BEGIN_RANGE; TypeIndex < VK_DESCRIPTOR_TYPE_END_RANGE; ++TypeIndex)
	{
		NumAllocatedTypes[TypeIndex] +=	(int32)Layout.GetTypesUsed((VkDescriptorType)TypeIndex);
		PeakAllocatedTypes[TypeIndex] = FMath::Max(PeakAllocatedTypes[TypeIndex], NumAllocatedTypes[TypeIndex]);
	}

	NumAllocatedDescriptorSets += Layout.GetLayouts().Num();
	PeakAllocatedDescriptorSets = FMath::Max(NumAllocatedDescriptorSets, PeakAllocatedDescriptorSets);
}

void FVulkanDescriptorPool::TrackRemoveUsage(const FVulkanDescriptorSetsLayout& Layout)
{
	for (uint32 TypeIndex = VK_DESCRIPTOR_TYPE_BEGIN_RANGE; TypeIndex < VK_DESCRIPTOR_TYPE_END_RANGE; ++TypeIndex)
	{
		NumAllocatedTypes[TypeIndex] -=	(int32)Layout.GetTypesUsed((VkDescriptorType)TypeIndex);
		check(NumAllocatedTypes[TypeIndex] >= 0);
	}

	NumAllocatedDescriptorSets -= Layout.GetLayouts().Num();
}

inline void FVulkanComputeShaderState::BindDescriptorSets(FVulkanCmdBuffer* Cmd)
{
	check(CurrDescriptorSets);
	CurrDescriptorSets->Bind(Cmd, GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);
}


inline void FVulkanBoundShaderState::BindDescriptorSets(FVulkanCmdBuffer* Cmd)
{
	check(CurrDescriptorSets);
	CurrDescriptorSets->Bind(Cmd, GetPipelineLayout(), VK_PIPELINE_BIND_POINT_GRAPHICS);
}

void FVulkanPendingComputeState::PrepareDispatch(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* Cmd)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDispatchCallPrepareTime);

	check(CurrentState.CSS);
	bool bHasDescriptorSets = CurrentState.CSS->UpdateDescriptorSets(CmdListContext, Cmd, GlobalUniformPool);

	FVulkanComputePipeline* Pipeline = CurrentState.CSS->PrepareForDispatch(CurrentState);

	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanPipelineBind);
		VkPipeline NewPipeline = Pipeline->GetHandle();
		CurrentState.CSS->BindPipeline(Cmd->GetHandle(), NewPipeline);
		if (bHasDescriptorSets)
		{
			CurrentState.CSS->BindDescriptorSets(Cmd);
		}
	}
}

void FVulkanPendingGfxState::PrepareDraw(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* Cmd, VkPrimitiveTopology Topology)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallPrepareTime);

	checkf(Topology < (1 << NUMBITS_POLYTYPE), TEXT("PolygonMode was too high a value for the PSO key [%d]"), Topology);
	SetKeyBits(CurrentKey, OFFSET_POLYTYPE, NUMBITS_POLYTYPE, Topology);

	check(CurrentState.BSS);
    bool bHasDescriptorSets = CurrentState.BSS->UpdateDescriptorSets(CmdListContext, Cmd, GlobalUniformPool);

	// let the BoundShaderState return a pipeline object for the full current state of things
	CurrentState.InputAssembly.topology = Topology;
	FVulkanGfxPipeline* Pipeline = CurrentState.BSS->PrepareForDraw(CmdListContext->GetCurrentRenderPass() ? CmdListContext->GetCurrentRenderPass() : CmdListContext->GetPreviousRenderPass(), CurrentKey, CurrentState.BSS->GetVertexInputStateInfo().GetHash(), CurrentState);

	check(Pipeline);

	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanPipelineBind);
		VkPipeline NewPipeline = Pipeline->GetHandle();
		CurrentState.BSS->BindPipeline(Cmd->GetHandle(), NewPipeline);
		Pipeline->UpdateDynamicStates(Cmd, CurrentState);
		if (bHasDescriptorSets)
		{
			CurrentState.BSS->BindDescriptorSets(Cmd);
		}
		CurrentState.BSS->BindVertexStreams(Cmd, PendingStreams);
	}
}

void FVulkanPendingGfxState::InitFrame()
{
}

void FVulkanPendingGfxState::SetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ)
{
	VkViewport& vp = CurrentState.Viewport;
	FMemory::Memzero(vp);

	vp.x = MinX;
	vp.y = MinY;
	vp.width = MaxX - MinX;
	vp.height = MaxY - MinY;

	// Engine parses in some cases MaxZ as 0.0, which is rubbish.
	if(MinZ == MaxZ)
	{
		vp.minDepth = MinZ;
		vp.maxDepth = MinZ + 1.0f;
	}
	else
	{
		vp.minDepth = MinZ;
		vp.maxDepth = MaxZ;
	}

	CurrentState.bNeedsViewportUpdate = true;

	// Set scissor to match the viewport when disabled
	if (!bScissorEnable)
	{
		SetScissorRect(vp.x, vp.y, vp.width, vp.height);
	}
}

void FVulkanPendingGfxState::SetScissor(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
{
	bScissorEnable = bEnable;

	if (bScissorEnable)
	{
		SetScissorRect(MinX, MinY, MaxX - MinX, MaxY - MinY);
	}
	else
	{
		const VkViewport& vp = CurrentState.Viewport;
		SetScissorRect(vp.x, vp.y, vp.width, vp.height);
	}
}

void FVulkanPendingGfxState::SetScissorRect(uint32 MinX, uint32 MinY, uint32 Width, uint32 Height)
{
	VkRect2D& Scissor = CurrentState.Scissor;
	FMemory::Memzero(Scissor);

	Scissor.offset.x = MinX;
	Scissor.offset.y = MinY;
	Scissor.extent.width = Width;
	Scissor.extent.height = Height;

	// todo vulkan: compare against previous (and viewport above)
	CurrentState.bNeedsScissorUpdate = true;
}

void FVulkanPendingGfxState::SetBoundShaderState(TRefCountPtr<FVulkanBoundShaderState> InBoundShaderState)
{
	check(InBoundShaderState);
	InBoundShaderState->ResetState();
	CurrentState.BSS = InBoundShaderState;
}

FVulkanBoundShaderState& FVulkanPendingGfxState::GetBoundShaderState()
{
	check(CurrentState.BSS);
	return *CurrentState.BSS;
}


void FVulkanPendingGfxState::SetBlendState(FVulkanBlendState* NewState)
{
	check(NewState);
	CurrentState.BlendState = NewState;

	// set blend modes into the key
	for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; Index++)
	{
		SetKeyBits(CurrentKey, BlendBitOffsets[Index], NUMBITS_BLEND_STATE, NewState->BlendStateKeys[Index]);
	}
}


void FVulkanPendingGfxState::SetDepthStencilState(FVulkanDepthStencilState* NewState, uint32 StencilRef)
{
	check(NewState);
	CurrentState.DepthStencilState = NewState;
	CurrentState.StencilRef = StencilRef;
	CurrentState.bNeedsStencilRefUpdate = true;

	SetKeyBits(CurrentKey, OFFSET_DEPTH_TEST_ENABLED, NUMBITS_DEPTH_TEST_ENABLED, NewState->DepthStencilState.depthTestEnable);
	SetKeyBits(CurrentKey, OFFSET_DEPTH_WRITE_ENABLED, NUMBITS_DEPTH_WRITE_ENABLED, NewState->DepthStencilState.depthWriteEnable);
	SetKeyBits(CurrentKey, OFFSET_DEPTH_COMPARE_OP, NUMBITS_DEPTH_COMPARE_OP, NewState->DepthStencilState.depthCompareOp);
	SetKeyBits(CurrentKey, OFFSET_STENCIL_TEST_ENABLED, NUMBITS_STENCIL_TEST_ENABLED, NewState->DepthStencilState.stencilTestEnable);
	SetKeyBits(CurrentKey, OFFSET_FRONT_STENCIL_OP, NUMBITS_FRONT_STENCIL_OP, NewState->FrontStencilKey);
	SetKeyBits(CurrentKey, OFFSET_BACK_STENCIL_OP, NUMBITS_BACK_STENCIL_OP, NewState->BackStencilKey);
}

void FVulkanPendingGfxState::SetRasterizerState(FVulkanRasterizerState* NewState)
{
	check(NewState);

	CurrentState.RasterizerState = NewState;

	// update running key
	SetKeyBits(CurrentKey, OFFSET_CULL_MODE, NUMBITS_CULL_MODE, NewState->RasterizerState.cullMode);
	SetKeyBits(CurrentKey, OFFSET_DEPTH_BIAS_ENABLED, NUMBITS_DEPTH_BIAS_ENABLED, NewState->RasterizerState.depthBiasEnable);
	SetKeyBits(CurrentKey, OFFSET_POLYFILL, NUMBITS_POLYFILL, NewState->RasterizerState.polygonMode == VK_POLYGON_MODE_FILL);
}
