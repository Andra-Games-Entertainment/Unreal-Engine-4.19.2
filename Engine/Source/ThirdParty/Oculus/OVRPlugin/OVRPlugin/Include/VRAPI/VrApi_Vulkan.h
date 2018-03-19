/************************************************************************************

Filename    :   VrApi_Vulkan.h
Content     :   Vulkan specific VrApi structures.
Created     :   October 2017
Authors     :   Gloria Kennickell
Language    :   C99

Copyright   :   Copyright 2017 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_VrApi_Vulkan_h
#define OVR_VrApi_Vulkan_h

#include "VrApi_Config.h"
#include "VrApi_Types.h"

#if defined( __cplusplus )
extern "C" {
#endif

// From <vulkan/vulkan.h>:
#if !defined(VK_VERSION_1_0)
#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__)) || \
    defined(_M_X64) || defined(__ia64) || defined(_M_IA64) || defined(__aarch64__) ||       \
    defined(__powerpc64__)
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef struct object##_T* object;
#else
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef uint64_t object;
#endif
VK_DEFINE_HANDLE(VkInstance)
VK_DEFINE_HANDLE(VkPhysicalDevice)
VK_DEFINE_HANDLE(VkDevice)
VK_DEFINE_HANDLE(VkQueue)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkImage)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkDeviceMemory)
#define VK_NULL_HANDLE 0
#endif

/// Returns a list of strings delimited by a single space identifying Vulkan extensions that must
/// be enabled for the instance in order for the VR runtime to support Vulkan-based applications.
OVR_VRAPI_EXPORT ovrResult vrapi_GetInstanceExtensionsVulkan( char * extensionNames, uint32_t * extensionNamesSize );

/// Returns a list of strings delimited by a single space identifying Vulkan extensions that must
/// be enabled for the device in order for the VR runtime to support Vulkan-based applications.
OVR_VRAPI_EXPORT ovrResult vrapi_GetDeviceExtensionsVulkan( char * extensionNames, uint32_t * extensionNamesSize );

/// Initialization parameters unique to Vulkan.
typedef struct
{
	VkInstance			Instance;
	VkPhysicalDevice	PhysicalDevice;
	VkDevice			Device;
} ovrSystemCreateInfoVulkan;

/// Initializes the API for Vulkan support.
/// This is lightweight and does not create any threads.
/// This is called after vrapi_Initialize and before texture swapchain creation, or
/// vrapi_enterVrMode.
OVR_VRAPI_EXPORT ovrResult vrapi_CreateSystemVulkan( ovrSystemCreateInfoVulkan * systemInfo );

/// Destroys the API for Vulkan support.
/// This is called before vrapi_Shutdown.
OVR_VRAPI_EXPORT void vrapi_DestroySystemVulkan();

/// Get the VkImage at the given index within the chain.
OVR_VRAPI_EXPORT VkImage vrapi_GetTextureSwapChainBufferVulkan( ovrTextureSwapChain * chain, int index );

#if defined( __cplusplus )
}	// extern "C"
#endif

#endif	// OVR_VrApi_Vulkan_h
