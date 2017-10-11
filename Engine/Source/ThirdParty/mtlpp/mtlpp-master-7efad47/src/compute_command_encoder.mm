/*
 * Copyright 2016-2017 Nikolay Aleksiev. All rights reserved.
 * License: https://github.com/naleksiev/mtlpp/blob/master/LICENSE
 */
// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// Modifications for Unreal Engine

#include "compute_command_encoder.hpp"
#include "buffer.hpp"
#include "compute_pipeline.hpp"
#include "sampler.hpp"
#include "heap.hpp"
#include <Metal/MTLComputeCommandEncoder.h>
#include <Metal/MTLHeap.h>
#include <Metal/MTLResource.h>

namespace mtlpp
{
    void ComputeCommandEncoder::SetComputePipelineState(const ComputePipelineState& state)
    {
        Validate();
        [(id<MTLComputeCommandEncoder>)m_ptr setComputePipelineState:(id<MTLComputePipelineState>)state.GetPtr()];
    }

    void ComputeCommandEncoder::SetBytes(const void* data, uint32_t length, uint32_t index)
    {
        Validate();
        [(id<MTLComputeCommandEncoder>)m_ptr setBytes:data length:length atIndex:index];
    }

    void ComputeCommandEncoder::SetBuffer(const Buffer& buffer, uint32_t offset, uint32_t index)
    {
        Validate();
        [(id<MTLComputeCommandEncoder>)m_ptr setBuffer:(id<MTLBuffer>)buffer.GetPtr() offset:offset atIndex:index];
    }

    void ComputeCommandEncoder::SetBufferOffset(uint32_t offset, uint32_t index)
    {
        Validate();
#if MTLPP_IS_AVAILABLE(10_11, 8_3)
        [(id<MTLComputeCommandEncoder>)m_ptr setBufferOffset:offset atIndex:index];
#endif
    }

    void ComputeCommandEncoder::SetBuffers(const Buffer* buffers, const uint64_t* offsets, const ns::Range& range)
    {
        Validate();

        const uint32_t maxBuffers = 32;
        assert(range.Length <= maxBuffers);

        id<MTLBuffer> mtlBuffers[maxBuffers];
        NSUInteger    nsOffsets[maxBuffers];
        for (uint32_t i=0; i<range.Length; i++)
        {
            mtlBuffers[i] = (id<MTLBuffer>)buffers[i].GetPtr();
            nsOffsets[i] = offsets[i];
        }

        [(id<MTLComputeCommandEncoder>)m_ptr setBuffers:mtlBuffers
                                                         offsets:nsOffsets
                                                       withRange:NSMakeRange(range.Location, range.Length)];
    }

    void ComputeCommandEncoder::SetTexture(const Texture& texture, uint32_t index)
    {
        Validate();
        [(id<MTLComputeCommandEncoder>)m_ptr setTexture:(id<MTLTexture>)texture.GetPtr() atIndex:index];
    }

    void ComputeCommandEncoder::SetTextures(const Texture* textures, const ns::Range& range)
    {
        Validate();

        const uint32_t maxTextures = 128;
        assert(range.Length <= maxTextures);

        id<MTLTexture> mtlTextures[maxTextures];
        for (uint32_t i=0; i<range.Length; i++)
            mtlTextures[i] = (id<MTLTexture>)textures[i].GetPtr();

        [(id<MTLComputeCommandEncoder>)m_ptr setTextures:mtlTextures
                                                        withRange:NSMakeRange(range.Location, range.Length)];
    }

    void ComputeCommandEncoder::SetSamplerState(const SamplerState& sampler, uint32_t index)
    {
        Validate();
        [(id<MTLComputeCommandEncoder>)m_ptr setSamplerState:(id<MTLSamplerState>)sampler.GetPtr() atIndex:index];
    }

    void ComputeCommandEncoder::SetSamplerStates(const SamplerState* samplers, const ns::Range& range)
    {
        Validate();

        const uint32_t maxStates = 16;
        assert(range.Length <= maxStates);

        id<MTLSamplerState> mtlStates[maxStates];
        for (uint32_t i=0; i<range.Length; i++)
            mtlStates[i] = (id<MTLSamplerState>)samplers[i].GetPtr();

        [(id<MTLComputeCommandEncoder>)m_ptr setSamplerStates:mtlStates
                                                             withRange:NSMakeRange(range.Location, range.Length)];
    }

    void ComputeCommandEncoder::SetSamplerState(const SamplerState& sampler, float lodMinClamp, float lodMaxClamp, uint32_t index)
    {
        Validate();
        [(id<MTLComputeCommandEncoder>)m_ptr setSamplerState:(id<MTLSamplerState>)sampler.GetPtr()
                                                          lodMinClamp:lodMinClamp
                                                          lodMaxClamp:lodMaxClamp
                                                              atIndex:index];
    }

    void ComputeCommandEncoder::SetSamplerStates(const SamplerState* samplers, const float* lodMinClamps, const float* lodMaxClamps, const ns::Range& range)
    {
        Validate();

        const uint32_t maxStates = 16;
        assert(range.Length <= maxStates);

        id<MTLSamplerState> mtlStates[maxStates];
        for (uint32_t i=0; i<range.Length; i++)
            mtlStates[i] = (id<MTLSamplerState>)samplers[i].GetPtr();

        [(id<MTLComputeCommandEncoder>)m_ptr setSamplerStates:mtlStates
                                                          lodMinClamps:lodMinClamps
                                                          lodMaxClamps:lodMaxClamps
                                                             withRange:NSMakeRange(range.Location, range.Length)];
    }

    void ComputeCommandEncoder::SetThreadgroupMemory(uint32_t length, uint32_t index)
    {
        Validate();
        [(id<MTLComputeCommandEncoder>)m_ptr setThreadgroupMemoryLength:length atIndex:index];
    }
	
	void ComputeCommandEncoder::SetImageblock(uint32_t width, uint32_t height)
	{
		Validate();
#if MTLPP_IS_AVAILABLE_IOS(11_0)
		[m_ptr setImageblockWidth:width height:height];
#endif
	}

    void ComputeCommandEncoder::SetStageInRegion(const Region& region)
    {
		Validate();
#if MTLPP_IS_AVAILABLE(10_12, 10_0)
        [(id<MTLComputeCommandEncoder>)m_ptr setStageInRegion:MTLRegionMake3D(region.Origin.X, region.Origin.Y, region.Origin.Z, region.Size.Width, region.Size.Height, region.Size.Depth)];
#endif
    }

    void ComputeCommandEncoder::DispatchThreadgroups(const Size& threadgroupsPerGrid, const Size& threadsPerThreadgroup)
    {
        Validate();
        MTLSize mtlThreadgroupsPerGrid = MTLSizeMake(threadgroupsPerGrid.Width, threadgroupsPerGrid.Height, threadgroupsPerGrid.Depth);
        MTLSize mtlThreadsPerThreadgroup = MTLSizeMake(threadsPerThreadgroup.Width, threadsPerThreadgroup.Height, threadsPerThreadgroup.Depth);
        [(id<MTLComputeCommandEncoder>)m_ptr dispatchThreadgroups:mtlThreadgroupsPerGrid threadsPerThreadgroup:mtlThreadsPerThreadgroup];
    }

    void ComputeCommandEncoder::DispatchThreadgroupsWithIndirectBuffer(const Buffer& indirectBuffer, uint32_t indirectBufferOffset, const Size& threadsPerThreadgroup)
    {
        Validate();
        MTLSize mtlThreadsPerThreadgroup = MTLSizeMake(threadsPerThreadgroup.Width, threadsPerThreadgroup.Height, threadsPerThreadgroup.Depth);
        [(id<MTLComputeCommandEncoder>)m_ptr dispatchThreadgroupsWithIndirectBuffer:(id<MTLBuffer>)indirectBuffer.GetPtr()
                                                                        indirectBufferOffset:indirectBufferOffset
                                                                       threadsPerThreadgroup:mtlThreadsPerThreadgroup];
    }
	
	void ComputeCommandEncoder::DispatchThreads(const Size& threadsPerGrid, const Size& threadsPerThreadgroup)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		MTLSize mtlThreadsPerGrid = MTLSizeMake(threadsPerGrid.Width, threadsPerGrid.Height, threadsPerGrid.Depth);
		MTLSize mtlThreadsPerThreadgroup = MTLSizeMake(threadsPerThreadgroup.Width, threadsPerThreadgroup.Height, threadsPerThreadgroup.Depth);
		[(id<MTLComputeCommandEncoder>)m_ptr dispatchThreads:mtlThreadsPerGrid threadsPerThreadgroup:mtlThreadsPerThreadgroup];
#endif
	}

    void ComputeCommandEncoder::UpdateFence(const Fence& fence)
    {
        Validate();
		if (@available(macOS 10.13, iOS 10.0, *))
			[(id<MTLComputeCommandEncoder>)m_ptr updateFence:(id<MTLFence>)fence.GetPtr()];
    }

    void ComputeCommandEncoder::WaitForFence(const Fence& fence)
    {
		Validate();
		if (@available(macOS 10.13, iOS 10.0, *))
			[(id<MTLComputeCommandEncoder>)m_ptr waitForFence:(id<MTLFence>)fence.GetPtr()];
    }
	
	void ComputeCommandEncoder::UseResource(const Resource& resource, ResourceUsage usage)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(id<MTLComputeCommandEncoder>)m_ptr useResource:(id<MTLResource>)resource.GetPtr() usage:(MTLResourceUsage)usage];
#endif
	}

	void ComputeCommandEncoder::UseResources(const Resource* resource, uint32_t count, ResourceUsage usage)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		id<MTLResource>* array = (id<MTLResource>*)alloca(count * sizeof(id<MTLResource>));
		for (uint32_t i = 0; i < count; i++)
			array[i] = (id<MTLResource>)resource[i].GetPtr();
			
		[(id<MTLComputeCommandEncoder>)m_ptr useResources:array count:count usage:(MTLResourceUsage)usage];
#endif
	}
	
	void ComputeCommandEncoder::UseHeap(const Heap& heap)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(id<MTLComputeCommandEncoder>)m_ptr useHeap:(id<MTLHeap>)heap.GetPtr()];
#endif
	}
	
	void ComputeCommandEncoder::UseHeaps(const Heap* heap, uint32_t count)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		id<MTLHeap>* array = (id<MTLHeap>*)alloca(count * sizeof(id<MTLHeap>));
		for (uint32_t i = 0; i < count; i++)
			array[i] = (id<MTLHeap>)heap[i].GetPtr();
		
		[(id<MTLComputeCommandEncoder>)m_ptr useHeaps:array count:count];
#endif
	}
}
