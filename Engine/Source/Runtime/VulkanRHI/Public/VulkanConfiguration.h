// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanConfiguration.h: Vulkan resource RHI definitions.
=============================================================================*/

// Compiled with 1.0.24.0

#pragma once

#include "RHIDefinitions.h"

// API version we want to target.
#if PLATFORM_WINDOWS
	#define UE_VK_API_VERSION	VK_MAKE_VERSION(1, 0, 1)
#elif PLATFORM_MAC // Needed for compiling Vulkan shaders for Android
	#define UE_VK_API_VERSION	VK_MAKE_VERSION(1, 0, 1)
#elif PLATFORM_ANDROID
	#define UE_VK_API_VERSION	VK_MAKE_VERSION(1, 0, 1)
#else
	#error Unsupported platform!
#endif

#if UE_BUILD_DEBUG || PLATFORM_WINDOWS
	#define VULKAN_HAS_DEBUGGING_ENABLED 1 //!!!
#else
	#define VULKAN_HAS_DEBUGGING_ENABLED 0
#endif

// constants we probably will change a few times
#define VULKAN_UB_RING_BUFFER_SIZE								(8 * 1024 * 1024)
#define VULKAN_TEMP_FRAME_ALLOCATOR_SIZE						(8 * 1024 * 1024)

enum class EDescriptorSetStage
{
	// Adjusting these requires a full shader rebuild (ie modify the guid on VulkanCommon.usf)
	Vertex		= 0,
	Pixel		= 1,
	Geometry	= 2,
	Hull		= 3,

	// Some devices only have 4 descriptor sets max
	MaxMobileSets	= 4,

	// This will make Tessellation not available on mobile
	Domain		= 4,

	// Compute is its own pipeline, so it can all live as set 0
	Compute		= 0,

	Invalid		= -1,
};

inline EDescriptorSetStage GetDescriptorSetForStage(EShaderFrequency Stage)
{
	switch (Stage)
	{
	case SF_Vertex:		return EDescriptorSetStage::Vertex;
	case SF_Hull:		return EDescriptorSetStage::Hull;
	case SF_Domain:		return EDescriptorSetStage::Domain;
	case SF_Pixel:		return EDescriptorSetStage::Pixel;
	case SF_Geometry:	return EDescriptorSetStage::Geometry;
	case SF_Compute:	return EDescriptorSetStage::Compute;
	default:
		checkf(0, TEXT("Invalid shader Stage %d"), Stage);
		break;
	}

	return EDescriptorSetStage::Invalid;
}

// Enables the VK_LAYER_LUNARG_api_dump layer and the report VK_DEBUG_REPORT_INFORMATION_BIT_EXT flag
#define VULKAN_ENABLE_API_DUMP									0
// Enables logging wrappers per Vulkan call
#define VULKAN_ENABLE_DUMP_LAYER								0
#define VULKAN_ENABLE_DRAW_MARKERS								PLATFORM_WINDOWS && !VULKAN_ENABLE_DUMP_LAYER
#define VULKAN_ALLOW_MIDPASS_CLEAR								0

// Keep the Vk*CreateInfo stored per object for debugging
#define VULKAN_KEEP_CREATE_INFO									0

#define VULKAN_SINGLE_ALLOCATION_PER_RESOURCE					0

#define VULKAN_CUSTOM_MEMORY_MANAGER_ENABLED					0
	

// This disables/overrides some if the graphics pipeline setup
// Please remove this after we are done with testing
#if PLATFORM_WINDOWS
	#define VULKAN_DISABLE_DEBUG_CALLBACK						0	/* Disable the DebugReportFunction() callback in VulkanDebug.cpp */
#else
	#define VULKAN_DISABLE_DEBUG_CALLBACK						1	/* Disable the DebugReportFunction() callback in VulkanDebug.cpp */
#endif

#define VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS						1

#define VULKAN_ENABLE_AGGRESSIVE_STATS							0

#define VULKAN_ENABLE_PIPELINE_CACHE							1

#define VULKAN_ENABLE_RHI_DEBUGGING								1

#define VULKAN_REUSE_FENCES										1

#if PLATFORM_ANDROID
	#define VULKAN_SIGNAL_UNIMPLEMENTED()
#else
	#define VULKAN_SIGNAL_UNIMPLEMENTED()				checkf(false, TEXT("Unimplemented vulkan functionality: %s"), TEXT(__FUNCTION__))
#endif

#if VULKAN_HAS_DEBUGGING_ENABLED
#else
	// Ensures all debug related defines are disabled
	#ifdef VULKAN_DISABLE_DEBUG_CALLBACK
		#undef VULKAN_DISABLE_DEBUG_CALLBACK
		#define VULKAN_DISABLE_DEBUG_CALLBACK 0
	#endif
#endif

namespace EVulkanBindingType
{
	enum EType : uint8
	{
		PackedUniformBuffer,		//VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
		UniformBuffer,			//VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER

		CombinedImageSampler,	//VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
		Sampler,					//VK_DESCRIPTOR_TYPE_SAMPLER
		Image,						//VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE

		UniformTexelBuffer,			//VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER	Buffer<>

		//A storage image (VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) is a descriptor type that is used for load, store, and atomic operations on image memory from within shaders bound to pipelines.
		StorageImage,				//VK_DESCRIPTOR_TYPE_STORAGE_IMAGE		RWTexture

		//RWBuffer/RWTexture?
		//A storage texel buffer (VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER) represents a tightly packed array of homogeneous formatted data that is stored in a buffer and is made accessible to shaders. Storage texel buffers differ from uniform texel buffers in that they support stores and atomic operations in shaders, may support a different maximum length, and may have different performance characteristics.
		StorageTexelBuffer,

		// UAV/RWBuffer
		//A storage buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) is a region of structured storage that supports both read and write access for shaders.In addition to general read and write operations, some members of storage buffers can be used as the target of atomic operations.In general, atomic operations are only supported on members that have unsigned integer formats.


		Count,
	};

	static inline char GetBindingTypeChar(EType Type)
	{
		// Make sure these do NOT alias EPackedTypeName*
		switch (Type)
		{
		case UniformBuffer:			return 'b';
		case CombinedImageSampler:	return 'c';
		case Sampler:				return 'p';
		case Image:					return 'w';
		case UniformTexelBuffer:	return 'x';
		case StorageImage:			return 'y';
		case StorageTexelBuffer:	return 'z';
		default:
			check(0);
			break;
		}

		return 0;
	}
}
