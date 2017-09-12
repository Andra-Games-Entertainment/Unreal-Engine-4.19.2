/*
 * Copyright 2016-2017 Nikolay Aleksiev. All rights reserved.
 * License: https://github.com/naleksiev/mtlpp/blob/master/LICENSE
 */
// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// Modifications for Unreal Engine

#include "device.hpp"
#include "buffer.hpp"
#include "command_queue.hpp"
#include "compute_pipeline.hpp"
#include "depth_stencil.hpp"
#include "render_pipeline.hpp"
#include "sampler.hpp"
#include "texture.hpp"
#include "heap.hpp"
#include "argument_encoder.hpp"
#include <Metal/MTLDevice.h>

namespace mtlpp
{
	ArgumentDescriptor::ArgumentDescriptor()
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
	: ns::Object(ns::Handle{ [MTLArgumentDescriptor new] }, false)
#endif
	{
	}
	
	DataType ArgumentDescriptor::GetDataType() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return (DataType)[(MTLArgumentDescriptor*)m_ptr dataType];
#else
		return 0;
#endif
	}
	
	uint32_t ArgumentDescriptor::GetIndex() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return [(MTLArgumentDescriptor*)m_ptr index];
#else
		return 0;
#endif
	}
	
	uint32_t ArgumentDescriptor::GetArrayLength() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return [(MTLArgumentDescriptor*)m_ptr arrayLength];
#else
		return 0;
#endif
	}
	
	ArgumentAccess ArgumentDescriptor::GetAccess() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return (ArgumentAccess)[(MTLArgumentDescriptor*)m_ptr access];
#else
		return 0;
#endif
	}
	
	TextureType ArgumentDescriptor::GetTextureType() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return (TextureType)[(MTLArgumentDescriptor*)m_ptr textureType];
#else
		return 0;
#endif
	}
	
	uint32_t ArgumentDescriptor::GetConstantBlockAlignment() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return [(MTLArgumentDescriptor*)m_ptr constantBlockAlignment];
#else
		return 0;
#endif
	}
	
    CompileOptions::CompileOptions() :
        ns::Object(ns::Handle{ (__bridge void*)[[MTLCompileOptions alloc] init] }, false)
    {
    }

	ns::String Device::GetWasAddedNotification()
	{
#if MTLPP_IS_AVAILABLE_MAC(10_13)
		return ns::Handle{ MTLDeviceWasAddedNotification };
#else
		return ns::String();
#endif
	}
	
	ns::String Device::GetRemovalRequestedNotification()
	{
#if MTLPP_IS_AVAILABLE_MAC(10_13)
		return ns::Handle{ MTLDeviceRemovalRequestedNotification };
#else
		return ns::String();
#endif
	}
	
	ns::String Device::GetWasRemovedNotification()
	{
#if MTLPP_IS_AVAILABLE_MAC(10_13)
		return ns::Handle{ MTLDeviceWasRemovedNotification };
#else
		return ns::String();
#endif
	}
	
	ns::Array<Device> Device::CopyAllDevicesWithObserver(ns::Object observer, std::function<void(const Device&, ns::String const&)> handler)
	{
#if MTLPP_IS_AVAILABLE_MAC(10_13)
		id<NSObject> obj = (id<NSObject>)observer.GetPtr();
		return ns::Handle{ MTLCopyAllDevicesWithObserver((id<NSObject>*)(obj ? &obj : nil), ^(id<MTLDevice>  _Nonnull device, MTLDeviceNotificationName  _Nonnull notifyName)
			{
			handler(ns::Handle{ device }, ns::String(ns::Handle{ notifyName }));
		}) };
#else
		return ns::Array<Device>();
#endif
	}
	
	void Device::RemoveDeviceObserver(ns::Object observer)
	{
#if MTLPP_IS_AVAILABLE_MAC(10_13)
		if (observer)
		{
			MTLRemoveDeviceObserver((id<NSObject>)observer.GetPtr());
		}
#endif
	}
	
    Device Device::CreateSystemDefaultDevice()
    {
        return ns::Handle{ (__bridge void*)MTLCreateSystemDefaultDevice() };
    }

    ns::Array<Device> Device::CopyAllDevices()
    {
#if MTLPP_PLATFORM_MAC
        return ns::Handle{ (__bridge void*)MTLCopyAllDevices() };
#else
        return ns::Handle{ nullptr };
#endif
    }

    ns::String Device::GetName() const
    {
        Validate();
        return ns::Handle{ (__bridge void*)[(__bridge id<MTLDevice>)m_ptr name] };
    }

    Size Device::GetMaxThreadsPerThreadgroup() const
    {
        Validate();
#if MTLPP_IS_AVAILABLE(10_11, 9_0)
        MTLSize mtlSize = [(__bridge id<MTLDevice>)m_ptr maxThreadsPerThreadgroup];
        return Size(uint32_t(mtlSize.width), uint32_t(mtlSize.height), uint32_t(mtlSize.depth));
#else
        return Size(0, 0, 0);
#endif
    }

    bool Device::IsLowPower() const
    {
        Validate();
#if MTLPP_PLATFORM_MAC
        return [(__bridge id<MTLDevice>)m_ptr isLowPower];
#else
        return false;
#endif
    }

    bool Device::IsHeadless() const
    {
        Validate();
#if MTLPP_PLATFORM_MAC
        return [(__bridge id<MTLDevice>)m_ptr isHeadless];
#else
        return false;
#endif
    }

    uint64_t Device::GetRecommendedMaxWorkingSetSize() const
    {
#if MTLPP_PLATFORM_MAC
		if(@available(macOS 10.12, *))
			return [(__bridge id<MTLDevice>)m_ptr recommendedMaxWorkingSetSize];
		else
#endif
        return 0;
    }

    bool Device::IsDepth24Stencil8PixelFormatSupported() const
    {
        Validate();
#if MTLPP_PLATFORM_MAC
        return [(__bridge id<MTLDevice>)m_ptr isDepth24Stencil8PixelFormatSupported];
#else
        return true;
#endif
    }
	
	uint64_t Device::GetRegistryID() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return [(id<MTLDevice>)m_ptr registryID];
#else
		return 0;
#endif
	}
	
	ReadWriteTextureTier Device::GetReadWriteTextureSupport() const
	{
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return (ReadWriteTextureTier)[(id<MTLDevice>)m_ptr readWriteTextureSupport];
#else
		return 0;
#endif
	}
	
	ArgumentBuffersTier Device::GetArgumentsBufferSupport() const
	{
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return (ArgumentBuffersTier)[(id<MTLDevice>)m_ptr argumentBuffersSupport];
#else
		return 0;
#endif
	}
	
	bool Device::AreRasterOrderGroupsSupported() const
	{
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return [(id<MTLDevice>)m_ptr areRasterOrderGroupsSupported];
#else
		return false;
#endif
	}
	
	uint64_t Device::GetCurrentAllocatedSize() const
	{
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return [(id<MTLDevice>)m_ptr currentAllocatedSize];
#else
		return 0;
#endif
	}

    CommandQueue Device::NewCommandQueue()
    {
        Validate();
        return ns::Handle{ (__bridge void*)[(__bridge id<MTLDevice>)m_ptr newCommandQueue] };
    }

    CommandQueue Device::NewCommandQueue(uint32_t maxCommandBufferCount)
    {
        Validate();
        return ns::Handle{ (__bridge void*)[(__bridge id<MTLDevice>)m_ptr newCommandQueueWithMaxCommandBufferCount:maxCommandBufferCount] };
    }

    SizeAndAlign Device::HeapTextureSizeAndAlign(const TextureDescriptor& desc)
    {
		if (@available(macOS 10.13, iOS 10.0, *))
		{
			MTLSizeAndAlign mtlSizeAndAlign = [(__bridge id<MTLDevice>)m_ptr heapTextureSizeAndAlignWithDescriptor:(__bridge MTLTextureDescriptor*)desc.GetPtr()];
			return SizeAndAlign{ uint32_t(mtlSizeAndAlign.size), uint32_t(mtlSizeAndAlign.align) };
		}
        return SizeAndAlign{0, 0};
    }

    SizeAndAlign Device::HeapBufferSizeAndAlign(uint32_t length, ResourceOptions options)
    {
		if (@available(macOS 10.13, iOS 10.0, *))
		{
			MTLSizeAndAlign mtlSizeAndAlign = [(__bridge id<MTLDevice>)m_ptr heapBufferSizeAndAlignWithLength:length options:MTLResourceOptions(options)];
			return SizeAndAlign{ uint32_t(mtlSizeAndAlign.size), uint32_t(mtlSizeAndAlign.align) };
		}
        return SizeAndAlign{0, 0};
    }

    Heap Device::NewHeap(const HeapDescriptor& descriptor)
    {
		if (@available(macOS 10.13, iOS 10.0, *))
		{
			return ns::Handle{ (__bridge void*)[(__bridge id<MTLDevice>)m_ptr newHeapWithDescriptor:(__bridge MTLHeapDescriptor*)descriptor.GetPtr()] };
		}
		return ns::Handle{ nullptr };
    }

    Buffer Device::NewBuffer(uint32_t length, ResourceOptions options)
    {
        Validate();
        return ns::Handle{ (__bridge void*)[(__bridge id<MTLDevice>)m_ptr newBufferWithLength:length options:MTLResourceOptions(options)] };
    }

    Buffer Device::NewBuffer(const void* pointer, uint32_t length, ResourceOptions options)
    {
        Validate();
        return ns::Handle{ (__bridge void*)[(__bridge id<MTLDevice>)m_ptr newBufferWithBytes:pointer length:length options:MTLResourceOptions(options)] };
    }


    Buffer Device::NewBuffer(void* pointer, uint32_t length, ResourceOptions options, std::function<void (void* pointer, uint32_t length)> deallocator)
    {
        Validate();
        return ns::Handle{
            (__bridge void*)[(__bridge id<MTLDevice>)m_ptr newBufferWithBytesNoCopy:pointer
                                                                             length:length
                                                                            options:MTLResourceOptions(options)
                                                                        deallocator:^(void* pointer, NSUInteger length) { deallocator(pointer, uint32_t(length)); }]
        };
    }

    DepthStencilState Device::NewDepthStencilState(const DepthStencilDescriptor& descriptor)
    {
        Validate();
        return ns::Handle{ (__bridge void*)[(__bridge id<MTLDevice>)m_ptr newDepthStencilStateWithDescriptor:(__bridge MTLDepthStencilDescriptor*)descriptor.GetPtr()] };
    }

    Texture Device::NewTexture(const TextureDescriptor& descriptor)
    {
        Validate();
        return ns::Handle{ (__bridge void*)[(__bridge id<MTLDevice>)m_ptr newTextureWithDescriptor:(__bridge MTLTextureDescriptor*)descriptor.GetPtr()] };
    }
	
	Texture Device::NewTextureWithDescriptor(const TextureDescriptor& descriptor, ns::IOSurface& iosurface, uint32_t plane)
	{
		Validate();
#if MTLPP_IS_AVAILABLE_MAC(10_11)
		return ns::Handle{ (__bridge void*)[(__bridge id<MTLDevice>)m_ptr newTextureWithDescriptor:(__bridge MTLTextureDescriptor*)descriptor.GetPtr() iosurface:(IOSurfaceRef)iosurface.GetPtr() plane:plane] };
#else
		return Texture();
#endif
	}

    SamplerState Device::NewSamplerState(const SamplerDescriptor& descriptor)
    {
        Validate();
        return ns::Handle{ (__bridge void*)[(__bridge id<MTLDevice>)m_ptr newSamplerStateWithDescriptor:(__bridge MTLSamplerDescriptor*)descriptor.GetPtr()] };
    }

    Library Device::NewDefaultLibrary()
    {
        Validate();
        return ns::Handle{ (__bridge void*)[(__bridge id<MTLDevice>)m_ptr newDefaultLibrary] };
    }

    Library Device::NewLibrary(const ns::String& filepath, ns::Error* error)
    {
        Validate();
        NSError* nsError = error ? (__bridge NSError*)error->GetPtr() : nullptr;
        return ns::Handle{ (__bridge void*)[(__bridge id<MTLDevice>)m_ptr newLibraryWithFile:(__bridge NSString*)filepath.GetPtr() error:&nsError] };
    }
	
	Library Device::NewDefaultLibraryWithBundle(const ns::Bundle& bundle, ns::Error* error)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_12, 10_0)
		NSError* nsError = error ? (__bridge NSError*)error->GetPtr() : nullptr;
		return ns::Handle{ (__bridge void*)[(__bridge id<MTLDevice>)m_ptr newDefaultLibraryWithBundle:(__bridge NSBundle*)bundle.GetPtr() error:&nsError] };
#else
		return Library();
#endif
	}

    Library Device::NewLibrary(const char* source, const CompileOptions& options, ns::Error* error)
    {
        Validate();
        NSString* nsSource = [NSString stringWithUTF8String:source];
        NSError* nsError = error ? (__bridge NSError*)error->GetPtr() : nullptr;
        return ns::Handle{
            (__bridge void*)[(__bridge id<MTLDevice>)m_ptr newLibraryWithSource:nsSource
                                                                        options:(__bridge MTLCompileOptions*)options.GetPtr()
                                                                          error:&nsError]
        };
    }
	
	Library Device::NewLibrary(ns::URL const& url, ns::Error* error)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		NSError* nsError = error ? (__bridge NSError*)error->GetPtr() : nullptr;
		return ns::Handle{ (__bridge void*)[(__bridge id<MTLDevice>)m_ptr newLibraryWithURL:(__bridge NSURL*)url.GetPtr() error:&nsError] };
#else
		return Library();
#endif
	}

    void Device::NewLibrary(const char* source, const CompileOptions& options, std::function<void(const Library&, const ns::Error&)> completionHandler)
    {
        Validate();
        NSString* nsSource = [NSString stringWithUTF8String:source];
        [(__bridge id<MTLDevice>)m_ptr newLibraryWithSource:nsSource
                                                    options:(__bridge MTLCompileOptions*)options.GetPtr()
                                          completionHandler:^(id <MTLLibrary> library, NSError * error) {
                                                completionHandler(
                                                    ns::Handle{ (__bridge void*)library },
                                                    ns::Handle{ (__bridge void*)error });
                                          }];
    }

    RenderPipelineState Device::NewRenderPipelineState(const RenderPipelineDescriptor& descriptor, ns::Error* error)
    {
        Validate();
        NSError* nsError = error ? (__bridge NSError*)error->GetPtr() : nullptr;
        return ns::Handle{
            (__bridge void*)[(__bridge id<MTLDevice>)m_ptr newRenderPipelineStateWithDescriptor:(__bridge MTLRenderPipelineDescriptor*)descriptor.GetPtr()
                                                                                          error:&nsError]
        };
    }

    RenderPipelineState Device::NewRenderPipelineState(const RenderPipelineDescriptor& descriptor, PipelineOption options, RenderPipelineReflection* outReflection, ns::Error* error)
    {
        Validate();
        NSError* nsError = error ? (__bridge NSError*)error->GetPtr() : nullptr;
        MTLRenderPipelineReflection* mtlReflection = outReflection ? (__bridge MTLRenderPipelineReflection*)outReflection->GetPtr() : nullptr;
        return ns::Handle{
            (__bridge void*)[(__bridge id<MTLDevice>)m_ptr newRenderPipelineStateWithDescriptor:(__bridge MTLRenderPipelineDescriptor*)descriptor.GetPtr()
                                                                                        options:MTLPipelineOption(options)
                                                                                     reflection:&mtlReflection
                                                                                          error:&nsError]
        };
    }

    void Device::NewRenderPipelineState(const RenderPipelineDescriptor& descriptor, std::function<void(const RenderPipelineState&, const ns::Error&)> completionHandler)
    {
        Validate();
        [(__bridge id<MTLDevice>)m_ptr newRenderPipelineStateWithDescriptor:(__bridge MTLRenderPipelineDescriptor*)descriptor.GetPtr()
                                                          completionHandler:^(id <MTLRenderPipelineState> renderPipelineState, NSError * error) {
                                                              completionHandler(
                                                                  ns::Handle{ (__bridge void*)renderPipelineState },
                                                                  ns::Handle{ (__bridge void*)error }
                                                              );
                                                          }];
    }

    void Device::NewRenderPipelineState(const RenderPipelineDescriptor& descriptor, PipelineOption options, std::function<void(const RenderPipelineState&, const RenderPipelineReflection&, const ns::Error&)> completionHandler)
    {
        Validate();
        [(__bridge id<MTLDevice>)m_ptr newRenderPipelineStateWithDescriptor:(__bridge MTLRenderPipelineDescriptor*)descriptor.GetPtr()
                                                                    options:MTLPipelineOption(options)
                                                          completionHandler:^(id <MTLRenderPipelineState> renderPipelineState, MTLRenderPipelineReflection * reflection, NSError * error) {
                                                              completionHandler(
                                                                  ns::Handle{ (__bridge void*)renderPipelineState },
                                                                  ns::Handle{ (__bridge void*)reflection },
                                                                  ns::Handle{ (__bridge void*)error }
                                                              );
                                                          }];
    }

    ComputePipelineState Device::NewComputePipelineState(const Function& computeFunction, ns::Error* error)
    {
        Validate();
        NSError* nsError = error ? (__bridge NSError*)error->GetPtr() : nullptr;
        return ns::Handle{
            (__bridge void*)[(__bridge id<MTLDevice>)m_ptr newComputePipelineStateWithFunction:(__bridge id<MTLFunction>)computeFunction.GetPtr()
                                                                                         error:&nsError]
        };
    }

    ComputePipelineState Device::NewComputePipelineState(const Function& computeFunction, PipelineOption options, ComputePipelineReflection& outReflection, ns::Error* error)
    {
        Validate();
        return ns::Handle{ nullptr };
    }

    void Device::NewComputePipelineState(const Function& computeFunction, std::function<void(const ComputePipelineState&, const ns::Error&)> completionHandler)
    {
        Validate();
        [(__bridge id<MTLDevice>)m_ptr newComputePipelineStateWithFunction:(__bridge id<MTLFunction>)computeFunction.GetPtr()
                                                         completionHandler:^(id <MTLComputePipelineState> computePipelineState, NSError * error) {
                                                             completionHandler(
                                                                 ns::Handle{ (__bridge void*)computePipelineState },
                                                                 ns::Handle{ (__bridge void*)error }
                                                             );
                                                         }];
    }

    void Device::NewComputePipelineState(const Function& computeFunction, PipelineOption options, std::function<void(const ComputePipelineState&, const ComputePipelineReflection&, const ns::Error&)> completionHandler)
    {
        Validate();
        [(__bridge id<MTLDevice>)m_ptr newComputePipelineStateWithFunction:(__bridge id<MTLFunction>)computeFunction.GetPtr()
                                                                   options:MTLPipelineOption(options)
                                                         completionHandler:^(id <MTLComputePipelineState> computePipelineState, MTLComputePipelineReflection * reflection, NSError * error) {
                                                             completionHandler(
                                                                 ns::Handle{ (__bridge void*)computePipelineState },
                                                                 ns::Handle{ (__bridge void*)reflection },
                                                                 ns::Handle{ (__bridge void*)error }
                                                             );
                                                         }];
    }

    ComputePipelineState Device::NewComputePipelineState(const ComputePipelineDescriptor& descriptor, PipelineOption options, ComputePipelineReflection* outReflection, ns::Error* error)
    {
        Validate();
#if MTLPP_IS_AVAILABLE(10_11, 9_0)
        NSError* nsError = error ? (__bridge NSError*)error->GetPtr() : nullptr;
        MTLComputePipelineReflection* mtlReflection = outReflection ? (__bridge MTLComputePipelineReflection*)outReflection->GetPtr() : nullptr;
        return ns::Handle{
            (__bridge void*)[(__bridge id<MTLDevice>)m_ptr newComputePipelineStateWithDescriptor:(__bridge MTLComputePipelineDescriptor*)descriptor.GetPtr()
                                                                                         options:MTLPipelineOption(options)
                                                                                      reflection:&mtlReflection
                                                                                           error:&nsError] };
#else
        return ns::Handle{ nullptr };
#endif
    }

    void Device::NewComputePipelineState(const ComputePipelineDescriptor& descriptor, PipelineOption options, std::function<void(const ComputePipelineState&, const ComputePipelineReflection&, const ns::Error&)> completionHandler)
    {
        Validate();
#if MTLPP_IS_AVAILABLE(10_11, 9_0)
        [(__bridge id<MTLDevice>)m_ptr newComputePipelineStateWithDescriptor:(__bridge MTLComputePipelineDescriptor*)descriptor.GetPtr()
                                                                     options:MTLPipelineOption(options)
                                                         completionHandler:^(id <MTLComputePipelineState> computePipelineState, MTLComputePipelineReflection * reflection, NSError * error)
                                                                    {
                                                                        completionHandler(
                                                                            ns::Handle{ (__bridge void*)computePipelineState },
                                                                            ns::Handle{ (__bridge void*)reflection },
                                                                            ns::Handle{ (__bridge void*)error });
                                                                    }];
#endif
    }

    Fence Device::NewFence()
    {
        Validate();
		if (@available(macOS 10.13, iOS 10.0, *))
			return ns::Handle{ (__bridge void*)[(__bridge id<MTLDevice>)m_ptr newFence] };
		else
			return ns::Handle{ nullptr };
    }

    bool Device::SupportsFeatureSet(FeatureSet featureSet) const
    {
        Validate();
        return [(__bridge id<MTLDevice>)m_ptr supportsFeatureSet:MTLFeatureSet(featureSet)];
    }

    bool Device::SupportsTextureSampleCount(uint32_t sampleCount) const
    {
        Validate();
#if MTLPP_IS_AVAILABLE(10_11, 9_0)
        return [(__bridge id<MTLDevice>)m_ptr supportsTextureSampleCount:sampleCount];
#else
        return true;
#endif
    }
	
	uint32_t Device::GetMinimumTextureAlignmentForPixelFormat(PixelFormat format) const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return [(__bridge id<MTLDevice>)m_ptr minimumLinearTextureAlignmentForPixelFormat:(MTLPixelFormat)format];
#else
		return 0;
#endif
	}
	
	uint32_t Device::GetMaxThreadgroupMemoryLength() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return [(__bridge id<MTLDevice>)m_ptr maxThreadgroupMemoryLength];
#else
		return 0;
#endif
	}
	
	bool Device::AreProgrammableSamplePositionsSupported() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return [(__bridge id<MTLDevice>)m_ptr areProgrammableSamplePositionsSupported];
#else
		return false;
#endif
	}
	
	void Device::GetDefaultSamplePositions(SamplePosition* positions, uint32_t count)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(__bridge id<MTLDevice>)m_ptr getDefaultSamplePositions:(MTLSamplePosition *)positions count:count];
#endif
	}
	
	ArgumentEncoder Device::NewArgumentEncoderWithArguments(ns::Array<ArgumentDescriptor> const& arguments)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ns::Handle{ [(__bridge id<MTLDevice>)m_ptr newArgumentEncoderWithArguments:(NSArray<MTLArgumentDescriptor *> *)arguments.GetPtr()] };
#else
		return ArgumentEncoder();
#endif
	}
}
