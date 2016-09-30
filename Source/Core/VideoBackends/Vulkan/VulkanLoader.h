// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#define VK_NO_PROTOTYPES

#if defined(WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(HAVE_X11)
// Currently we're getting xlib handles passed to the backend.
// If this ever changes to xcb, it's a simple change here.
#define VK_USE_PLATFORM_XLIB_KHR
//#define VK_USE_PLATFORM_XCB_KHR
#elif defined(ANDROID)
#define VK_USE_PLATFORM_ANDROID_KHR
#else
//#warning Unknown platform
#endif

#include "vulkan/vulkan.h"

// We abuse the preprocessor here to only need to specify function names once.
#define VULKAN_MODULE_ENTRY_POINT(name, required) extern PFN_##name name;
#define VULKAN_INSTANCE_ENTRY_POINT(name, required) extern PFN_##name name;
#define VULKAN_DEVICE_ENTRY_POINT(name, required) extern PFN_##name name;
#include "VideoBackends/Vulkan/VulkanEntryPoints.inl"
#undef VULKAN_DEVICE_ENTRY_POINT
#undef VULKAN_INSTANCE_ENTRY_POINT
#undef VULKAN_MODULE_ENTRY_POINT

namespace Vulkan
{
bool LoadVulkanLibrary();
bool LoadVulkanInstanceFunctions(VkInstance instance);
bool LoadVulkanDeviceFunctions(VkDevice device);
void UnloadVulkanLibrary();

const char* VkResultToString(VkResult res);
void LogVulkanResult(int level, const char* func_name, VkResult res, const char* msg, ...);

#define LOG_VULKAN_ERROR(res, ...) LogVulkanResult(2, __FUNCTION__, res, __VA_ARGS__)

}  // namespace Vulkan
