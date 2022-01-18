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
#pragma warning(disable : 4127)  // conditional expression is constant
#pragma warning(disable : 4100)  // unreferenced formal parameter
#pragma warning(disable : 4189)  // local variable is initialized but not referenced
#pragma warning(disable : 4324)  // structure was padded due to alignment specifier

#endif  // #ifdef _MSVC_LANG

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored                                                                   \
    "-Wtautological-compare"  // comparison of unsigned expression < 0 is always false
#pragma clang diagnostic ignored "-Wunused-private-field"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif

#include "vk_mem_alloc.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifdef _MSVC_LANG
#pragma warning(pop)
#endif

#include "Common/Logging/Log.h"

namespace Vulkan
{
bool LoadVulkanLibrary();
bool LoadVulkanInstanceFunctions(VkInstance instance);
bool LoadVulkanDeviceFunctions(VkDevice device);
void UnloadVulkanLibrary();

const char* VkResultToString(VkResult res);
void LogVulkanResult(Common::Log::LogLevel level, const char* func_name, VkResult res,
                     const char* msg, ...);

#define LOG_VULKAN_ERROR(res, ...)                                                                 \
  LogVulkanResult(Common::Log::LogLevel::LERROR, __func__, res, __VA_ARGS__)

}  // namespace Vulkan
