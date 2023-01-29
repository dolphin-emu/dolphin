// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define VK_NO_PROTOTYPES

#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#if defined(HAVE_X11)
#define VK_USE_PLATFORM_XLIB_KHR
#endif

#if defined(ANDROID)
#define VK_USE_PLATFORM_ANDROID_KHR
#endif

#if defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#endif

#include "vulkan/vulkan.h"

// Currently, exclusive fullscreen is only supported on Windows.
#if defined(WIN32)
#define SUPPORTS_VULKAN_EXCLUSIVE_FULLSCREEN 1
#endif

// We abuse the preprocessor here to only need to specify function names once.
#define VULKAN_MODULE_ENTRY_POINT(name, required) extern PFN_##name name;
#define VULKAN_INSTANCE_ENTRY_POINT(name, required) extern PFN_##name name;
#define VULKAN_DEVICE_ENTRY_POINT(name, required) extern PFN_##name name;
#include "VideoBackends/Vulkan/VulkanEntryPoints.inl"
#undef VULKAN_DEVICE_ENTRY_POINT
#undef VULKAN_INSTANCE_ENTRY_POINT
#undef VULKAN_MODULE_ENTRY_POINT

// Include vma allocator globally since including it before the vulkan headers causes
// errors
#ifdef _MSVC_LANG
#pragma warning(push, 4)
#pragma warning(disable : 4189)  // local variable is initialized but not referenced

#endif  // #ifdef _MSVC_LANG

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wnullability-completeness"
#endif  // #ifdef __clang__

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif  // #ifdef __GNUC__

#define VMA_VULKAN_VERSION 1001000
#define VMA_STATIC_VULKAN_FUNCTIONS 1
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#undef VK_NO_PROTOTYPES
#include "vk_mem_alloc.h"

#ifdef _MSVC_LANG
#pragma warning(pop)
#endif  // #ifdef _MSVC_LANG

#ifdef __clang__
#pragma clang diagnostic pop
#endif  // #ifdef __clang__

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // #ifdef __GNUC__

#include "Common/Logging/Log.h"

namespace Vulkan
{
bool LoadVulkanLibrary();
bool LoadVulkanInstanceFunctions(VkInstance instance);
bool LoadVulkanDeviceFunctions(VkDevice device);
void UnloadVulkanLibrary();

const char* VkResultToString(VkResult res);
void LogVulkanResult(Common::Log::LogLevel level, const char* func_name, VkResult res,
                     const char* msg);

#define LOG_VULKAN_ERROR(res, msg)                                                                 \
  LogVulkanResult(Common::Log::LogLevel::LERROR, __func__, res, msg)

}  // namespace Vulkan
