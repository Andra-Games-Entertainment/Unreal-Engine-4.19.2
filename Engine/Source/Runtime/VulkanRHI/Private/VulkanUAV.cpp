// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VulkanRHIPrivate.h"
#include "VulkanContext.h"

FVulkanShaderResourceView::~FVulkanShaderResourceView()
{
	TextureView.Destroy(*Device);
	BufferView = nullptr;
	SourceVertexBuffer = nullptr;
	SourceTexture = nullptr;
	Device = nullptr;
}

void FVulkanShaderResourceView::UpdateView()
{
	// update the buffer view for dynamic VB backed buffers (or if it was never set)
	if (SourceVertexBuffer != nullptr)
	{
		if (SourceVertexBuffer->IsVolatile() && VolatileLockCounter != SourceVertexBuffer->GetVolatileLockCounter())
		{
			BufferView = nullptr;
			VolatileLockCounter = SourceVertexBuffer->GetVolatileLockCounter();
		}

		if (BufferView == nullptr || SourceVertexBuffer->IsDynamic())
		{
			SCOPE_CYCLE_COUNTER(STAT_VulkanSRVUpdateTime);
			// thanks to ref counting, overwriting the buffer will toss the old view
			BufferView = new FVulkanBufferView(Device);
			BufferView->Create(SourceVertexBuffer.GetReference(), BufferViewFormat, SourceVertexBuffer->GetOffset(), SourceVertexBuffer->GetSize());
		}
	}
	else
	{
		if (TextureView.View == VK_NULL_HANDLE)
		{
			EPixelFormat Format = (BufferViewFormat == PF_Unknown) ? SourceTexture->GetFormat() : BufferViewFormat;
			if (FRHITexture2D* Tex2D = SourceTexture->GetTexture2D())
			{
				FVulkanTexture2D* VTex2D = ResourceCast(Tex2D);
				if (Format == PF_X24_G8)
				{
					Format = PF_DepthStencil;
				}
				TextureView.Create(*Device, VTex2D->Surface.Image, VK_IMAGE_VIEW_TYPE_2D, VTex2D->Surface.GetPartialAspectMask(), Format, UEToVkFormat(Format, false), MipLevel, NumMips, 0, 1);
			}
			else if (FRHITextureCube* TexCube = SourceTexture->GetTextureCube())
			{
				FVulkanTextureCube* VTexCube = ResourceCast(TexCube);
				if (Format == PF_X24_G8)
				{
					Format = PF_DepthStencil;
				}
				TextureView.Create(*Device, VTexCube->Surface.Image, VK_IMAGE_VIEW_TYPE_CUBE, VTexCube->Surface.GetPartialAspectMask(), Format, UEToVkFormat(Format, false), MipLevel, NumMips, 0, 1);
			}
			else
			{
				ensure(0);
			}
		}
	}
}

void FVulkanUnorderedAccessView::UpdateView()
{
	// update the buffer view for dynamic VB backed buffers (or if it was never set)
	if (SourceVertexBuffer != nullptr)
	{
		if (SourceVertexBuffer->IsVolatile() && VolatileLockCounter != SourceVertexBuffer->GetVolatileLockCounter())
		{
			BufferView = nullptr;
			VolatileLockCounter = SourceVertexBuffer->GetVolatileLockCounter();
		}

		if (BufferView == nullptr || SourceVertexBuffer->IsDynamic())
		{
			SCOPE_CYCLE_COUNTER(STAT_VulkanSRVUpdateTime);
			// thanks to ref counting, overwriting the buffer will toss the old view
			BufferView = new FVulkanBufferView(Device);
			BufferView->Create(SourceVertexBuffer.GetReference(), BufferViewFormat, SourceVertexBuffer->GetOffset(), SourceVertexBuffer->GetSize());
		}
	}
	else
	{
		if (TextureView.View == VK_NULL_HANDLE)
		{
			EPixelFormat Format = (BufferViewFormat == PF_Unknown) ? SourceTexture->GetFormat() : BufferViewFormat;
			if (FRHITexture2D* Tex2D = SourceTexture->GetTexture2D())
			{
				FVulkanTexture2D* VTex2D = ResourceCast(Tex2D);
				if (Format == PF_X24_G8)
				{
					Format = PF_DepthStencil;
				}
				TextureView.Create(*Device, VTex2D->Surface.Image, VK_IMAGE_VIEW_TYPE_2D, VTex2D->Surface.GetPartialAspectMask(), Format, UEToVkFormat(Format, false), MipLevel, 1, 0, 1);
			}
			else
			{
				ensure(0);
			}
		}
	}
}

FUnorderedAccessViewRHIRef FVulkanDynamicRHI::RHICreateUnorderedAccessView(FStructuredBufferRHIParamRef StructuredBufferRHI, bool bUseUAVCounter, bool bAppendBuffer)
{
#if 0
	FVulkanStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);

	// create the UAV buffer to point to the structured buffer's memory
	FVulkanUnorderedAccessView* UAV = new FVulkanUnorderedAccessView;
	UAV->SourceStructuredBuffer = StructuredBuffer;

	return UAV;
#else
	VULKAN_SIGNAL_UNIMPLEMENTED();
	return nullptr;
#endif
}

FUnorderedAccessViewRHIRef FVulkanDynamicRHI::RHICreateUnorderedAccessView(FTextureRHIParamRef TextureRHI, uint32 MipLevel)
{
#if 0
	FVulkanTextureBase* Base = nullptr;
	if (auto* Tex2D = TextureRHI->GetTexture2D())
	{
		Base = ResourceCast(Tex2D);
	}
	else if (auto* Tex3D = TextureRHI->GetTexture3D())
	{
		Base = ResourceCast(Tex3D);
	}
	else if (auto* TexCube = TextureRHI->GetTextureCube())
	{
		Base = ResourceCast(TexCube);
	}
	else
	{
		ensure(0);
	}
#endif
	FVulkanUnorderedAccessView* UAV = new FVulkanUnorderedAccessView(Device);
	UAV->SourceTexture = TextureRHI;
	UAV->MipLevel = MipLevel;
	return UAV;
}

FUnorderedAccessViewRHIRef FVulkanDynamicRHI::RHICreateUnorderedAccessView(FVertexBufferRHIParamRef VertexBufferRHI, uint8 Format)
{
	FVulkanVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);

	FVulkanUnorderedAccessView* UAV = new FVulkanUnorderedAccessView(Device);
	// delay the shader view create until we use it, so we just track the source info here
	UAV->BufferViewFormat = (EPixelFormat)Format;
	UAV->SourceVertexBuffer = VertexBuffer;

	return UAV;
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FStructuredBufferRHIParamRef StructuredBufferRHI)
{
#if 0
	FVulkanStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);

	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView;
	return SRV;
#else
	VULKAN_SIGNAL_UNIMPLEMENTED();
	return nullptr;
#endif
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FVertexBufferRHIParamRef VertexBufferRHI, uint32 Stride, uint8 Format)
{	
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView(Device);
	// delay the shader view create until we use it, so we just track the source info here
	SRV->SourceVertexBuffer = ResourceCast(VertexBufferRHI);
	SRV->BufferViewFormat = (EPixelFormat)Format;
	return SRV;
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel)
{
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView(Device);
	// delay the shader view create until we use it, so we just track the source info here
	SRV->SourceTexture = ResourceCast(Texture2DRHI);
	SRV->MipLevel = MipLevel;
	SRV->NumMips = 1;
	return SRV;
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format)
{
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView(Device);
	// delay the shader view create until we use it, so we just track the source info here
	SRV->SourceTexture = ResourceCast(Texture2DRHI);
	SRV->BufferViewFormat = (EPixelFormat)Format;
	SRV->MipLevel = MipLevel;
	SRV->NumMips = NumMipLevels;
	return SRV;
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel)
{
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView(Device);
	// delay the shader view create until we use it, so we just track the source info here
	SRV->SourceTexture = ResourceCast(Texture3DRHI);
	SRV->MipLevel = MipLevel;
	SRV->NumMips = 1;
	return SRV;
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel)
{
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView(Device);
	// delay the shader view create until we use it, so we just track the source info here
	SRV->SourceTexture = ResourceCast(Texture2DArrayRHI);
	SRV->MipLevel = MipLevel;
	SRV->NumMips = 1;
	return SRV;
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel)
{
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView(Device);
	// delay the shader view create until we use it, so we just track the source info here
	SRV->SourceTexture = ResourceCast(TextureCubeRHI);
	SRV->MipLevel = MipLevel;
	SRV->NumMips = 1;
	return SRV;
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FIndexBufferRHIParamRef Buffer)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
	return nullptr;
}

void FVulkanCommandListContext::RHIClearTinyUAV(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const uint32* Values)
{
	FVulkanUnorderedAccessView* UnorderedAccessView = ResourceCast(UnorderedAccessViewRHI);
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	ensure(CmdBuffer->IsOutsideRenderPass());
	if (UnorderedAccessView->SourceVertexBuffer)
	{
		FVulkanVertexBuffer* VertexBuffer = UnorderedAccessView->SourceVertexBuffer;
		switch (UnorderedAccessView->BufferViewFormat)
		{
		case PF_R32_SINT:
		case PF_R32_FLOAT:
		case PF_R32_UINT:
			break;
		case PF_A8R8G8B8:
		case PF_R8G8B8A8:
		case PF_B8G8R8A8:
			ensure(Values[0] == Values[1] && Values[1] == Values[2] && Values[2] == Values[3]);
			break;
		default:
			ensureMsgf(0, TEXT("Unsupported format (EPixelFormat)%d!"), (uint32)UnorderedAccessView->BufferViewFormat);
			break;
		}
		VulkanRHI::vkCmdFillBuffer(CmdBuffer->GetHandle(), VertexBuffer->GetHandle(), VertexBuffer->GetOffset(), VertexBuffer->GetSize(), Values[0]);
	}
	else
	{
		ensure(0);
	}
}

FVulkanComputeFence::FVulkanComputeFence(FVulkanDevice* InDevice, FName InName)
	: FRHIComputeFence(InName)
	, VulkanRHI::FGPUEvent(InDevice)
{
}

FVulkanComputeFence::~FVulkanComputeFence()
{
}

void FVulkanComputeFence::WriteCmd(VkCommandBuffer CmdBuffer)
{
	FRHIComputeFence::WriteFence();
	VulkanRHI::vkCmdSetEvent(CmdBuffer, Handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}


FComputeFenceRHIRef FVulkanDynamicRHI::RHICreateComputeFence(const FName& Name)
{
	return new FVulkanComputeFence(Device, Name);
}

void FVulkanCommandListContext::RHIWaitComputeFence(FComputeFenceRHIParamRef InFence)
{
	FVulkanComputeFence* Fence = ResourceCast(InFence);
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	VkEvent Event = Fence->GetHandle();
	VulkanRHI::vkCmdWaitEvents(CmdBuffer->GetHandle(), 1, &Event, VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_ALL_GRAPHICS, 0, nullptr, 0, nullptr, 0, nullptr);
	IRHICommandContext::RHIWaitComputeFence(InFence);
}