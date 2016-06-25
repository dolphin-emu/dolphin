// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#define VK_NO_PROTOTYPES

#if defined(WIN32)
	#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(HAVE_X11)
	#define VK_USE_PLATFORM_XLIB_KHR
#else
	#warning Unknown platform
#endif

#include <vulkan/vulkan.h>

// We abuse the preprocessor here to only need to specify function names once.
#define VULKAN_ENTRY_POINT(type, name, required) extern type name;
#include "VideoBackends/Vulkan/VulkanImports.inl"
#undef VULKAN_ENTRY_POINT

namespace Vulkan {

bool LoadVulkanLibrary();
void UnloadVulkanLibrary();

const char* VkResultToString(VkResult res);
void LogVulkanResult(int level, const char* func_name, VkResult res, const char* msg, ...);

#define LOG_VULKAN_ERROR(res, ...) LogVulkanResult(2, __FUNCTION__, res, __VA_ARGS__)

}       // namespace Vulkan
