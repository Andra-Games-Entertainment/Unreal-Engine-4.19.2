/*
 * Copyright 2016-2017 Nikolay Aleksiev. All rights reserved.
 * License: https://github.com/naleksiev/mtlpp/blob/master/LICENSE
 */
// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// Modifications for Unreal Engine

#include "command_buffer.hpp"
#include "command_queue.hpp"
#include "drawable.hpp"
#include "blit_command_encoder.hpp"
#include "render_command_encoder.hpp"
#include "compute_command_encoder.hpp"
#include "parallel_render_command_encoder.hpp"
#include "render_pass.hpp"

#include <Metal/MTLCommandBuffer.h>

namespace mtlpp
{
    Device CommandBuffer::GetDevice() const
    {
        Validate();
        return [(id<MTLCommandBuffer>)m_ptr device];
    }

    CommandQueue CommandBuffer::GetCommandQueue() const
    {
        Validate();
        return [(id<MTLCommandBuffer>)m_ptr commandQueue];
    }

    bool CommandBuffer::GetRetainedReferences() const
    {
        Validate();
        return [(id<MTLCommandBuffer>)m_ptr retainedReferences];
    }

    ns::String CommandBuffer::GetLabel() const
    {
        Validate();
        return [(id<MTLCommandBuffer>)m_ptr label];
    }

    CommandBufferStatus CommandBuffer::GetStatus() const
    {
        Validate();
        return CommandBufferStatus([(id<MTLCommandBuffer>)m_ptr status]);
    }

    ns::Error CommandBuffer::GetError() const
    {
        Validate();
        return [(id<MTLCommandBuffer>)m_ptr error];
    }

    double CommandBuffer::GetKernelStartTime() const
    {
        Validate();
#if MTLPP_PLATFORM_IOS
		if(@available(iOS 10.3, *))
			return [(id<MTLCommandBuffer>)m_ptr kernelStartTime];
		else
#endif
		return 0.0;
	}

    double CommandBuffer::GetKernelEndTime() const
    {
        Validate();
#if MTLPP_PLATFORM_IOS
        return [(id<MTLCommandBuffer>)m_ptr kernelEndTime];
#else
        return 0.0;
#endif
    }

    double CommandBuffer::GetGpuStartTime() const
    {
        Validate();
#if MTLPP_PLATFORM_IOS
        return [(id<MTLCommandBuffer>)m_ptr GPUStartTime];
#else
        return 0.0;
#endif
    }

    double CommandBuffer::GetGpuEndTime() const
    {
        Validate();
#if MTLPP_PLATFORM_IOS
        return [(id<MTLCommandBuffer>)m_ptr GPUEndTime];
#else
        return 0.0;
#endif
    }

    void CommandBuffer::SetLabel(const ns::String& label)
    {
        Validate();
        [(id<MTLCommandBuffer>)m_ptr setLabel:(NSString*)label.GetPtr()];
    }

    void CommandBuffer::Enqueue()
    {
        Validate();
        [(id<MTLCommandBuffer>)m_ptr enqueue];
    }

    void CommandBuffer::Commit()
    {
        Validate();
        [(id<MTLCommandBuffer>)m_ptr commit];
    }

    void CommandBuffer::AddScheduledHandler(std::function<void(const CommandBuffer&)> handler)
    {
        Validate();
        [(id<MTLCommandBuffer>)m_ptr addScheduledHandler:^(id <MTLCommandBuffer> mtlCommandBuffer){
            CommandBuffer commandBuffer(mtlCommandBuffer);
            handler(commandBuffer);
        }];
    }

    void CommandBuffer::AddCompletedHandler(std::function<void(const CommandBuffer&)> handler)
    {
        Validate();
        [(id<MTLCommandBuffer>)m_ptr addCompletedHandler:^(id <MTLCommandBuffer> mtlCommandBuffer){
            CommandBuffer commandBuffer(mtlCommandBuffer);
            handler(commandBuffer);
        }];
    }

    void CommandBuffer::Present(const Drawable& drawable)
    {
        Validate();
        [(id<MTLCommandBuffer>)m_ptr presentDrawable:(id<MTLDrawable>)drawable.GetPtr()];
    }

    void CommandBuffer::PresentAtTime(const Drawable& drawable, double presentationTime)
    {
        Validate();
        [(id<MTLCommandBuffer>)m_ptr presentDrawable:(id<MTLDrawable>)drawable.GetPtr() atTime:presentationTime];
    }

    void CommandBuffer::PresentAfterMinimumDuration(const Drawable& drawable, double duration)
    {
        Validate();
#if MTLPP_PLATFORM_IOS
        [(id<MTLCommandBuffer>)m_ptr presentDrawable:(id<MTLDrawable>)drawable.GetPtr() afterMinimumDuration:duration];
#endif
    }

    void CommandBuffer::WaitUntilScheduled()
    {
        Validate();
        [(id<MTLCommandBuffer>)m_ptr waitUntilScheduled];
    }

    void CommandBuffer::WaitUntilCompleted()
    {
        Validate();
        [(id<MTLCommandBuffer>)m_ptr waitUntilCompleted];
    }

    BlitCommandEncoder CommandBuffer::BlitCommandEncoder()
    {
        Validate();
        return [(id<MTLCommandBuffer>)m_ptr blitCommandEncoder];
    }

    RenderCommandEncoder CommandBuffer::RenderCommandEncoder(const RenderPassDescriptor& renderPassDescriptor)
    {
        Validate();
        MTLRenderPassDescriptor* mtlRenderPassDescriptor = (MTLRenderPassDescriptor*)renderPassDescriptor.GetPtr();
        return [(id<MTLCommandBuffer>)m_ptr renderCommandEncoderWithDescriptor:mtlRenderPassDescriptor];
    }

    ComputeCommandEncoder CommandBuffer::ComputeCommandEncoder()
    {
        Validate();
        return [(id<MTLCommandBuffer>)m_ptr computeCommandEncoder];
    }

    ParallelRenderCommandEncoder CommandBuffer::ParallelRenderCommandEncoder(const RenderPassDescriptor& renderPassDescriptor)
    {
        Validate();
        MTLRenderPassDescriptor* mtlRenderPassDescriptor = (MTLRenderPassDescriptor*)renderPassDescriptor.GetPtr();
        return [(id<MTLCommandBuffer>)m_ptr parallelRenderCommandEncoderWithDescriptor:mtlRenderPassDescriptor];
    }
	
	void CommandBuffer::PushDebugGroup(const ns::String& string)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(id<MTLCommandBuffer>)m_ptr pushDebugGroup:(NSString *)string.GetPtr()];
#endif
	}
	
	void CommandBuffer::PopDebugGroup()
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(id<MTLCommandBuffer>)m_ptr popDebugGroup];
#endif
	}
}
