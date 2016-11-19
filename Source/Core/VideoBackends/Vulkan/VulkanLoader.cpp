// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <atomic>
#include <cstdarg>

#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#include "VideoBackends/Vulkan/VulkanLoader.h"

#if defined(VK_USE_PLATFORM_WIN32_KHR)
#include <Windows.h>
#elif defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_XCB_KHR) ||                     \
    defined(VK_USE_PLATFORM_ANDROID_KHR)
#include <dlfcn.h>
#endif

#define VULKAN_MODULE_ENTRY_POINT(name, required) PFN_##name name;
#define VULKAN_INSTANCE_ENTRY_POINT(name, required) PFN_##name name;
#define VULKAN_DEVICE_ENTRY_POINT(name, required) PFN_##name name;
#include "VideoBackends/Vulkan/VulkanEntryPoints.inl"
#undef VULKAN_DEVICE_ENTRY_POINT
#undef VULKAN_INSTANCE_ENTRY_POINT
#undef VULKAN_MODULE_ENTRY_POINT

namespace Vulkan
{
static void ResetVulkanLibraryFunctionPointers()
{
#define VULKAN_MODULE_ENTRY_POINT(name, required) name = nullptr;
#define VULKAN_INSTANCE_ENTRY_POINT(name, required) name = nullptr;
#define VULKAN_DEVICE_ENTRY_POINT(name, required) name = nullptr;
#include "VideoBackends/Vulkan/VulkanEntryPoints.inl"
#undef VULKAN_DEVICE_ENTRY_POINT
#undef VULKAN_INSTANCE_ENTRY_POINT
#undef VULKAN_MODULE_ENTRY_POINT
}

#if defined(VK_USE_PLATFORM_WIN32_KHR)

static HMODULE vulkan_module;
static std::atomic_int vulkan_module_ref_count = {0};

bool LoadVulkanLibrary()
{
  // Not thread safe if a second thread calls the loader whilst the first is still in-progress.
  if (vulkan_module)
  {
    vulkan_module_ref_count++;
    return true;
  }

  vulkan_module = LoadLibraryA("vulkan-1.dll");
  if (!vulkan_module)
  {
    ERROR_LOG(VIDEO, "Failed to load vulkan-1.dll");
    return false;
  }

  bool required_functions_missing = false;
  auto LoadFunction = [&](FARPROC* func_ptr, const char* name, bool is_required) {
    *func_ptr = GetProcAddress(vulkan_module, name);
    if (!(*func_ptr) && is_required)
    {
      ERROR_LOG(VIDEO, "Vulkan: Failed to load required module function %s", name);
      required_functions_missing = true;
    }
  };

#define VULKAN_MODULE_ENTRY_POINT(name, required)                                                  \
  LoadFunction(reinterpret_cast<FARPROC*>(&name), #name, required);
#include "VideoBackends/Vulkan/VulkanEntryPoints.inl"
#undef VULKAN_MODULE_ENTRY_POINT

  if (required_functions_missing)
  {
    ResetVulkanLibraryFunctionPointers();
    FreeLibrary(vulkan_module);
    vulkan_module = nullptr;
    return false;
  }

  vulkan_module_ref_count++;
  return true;
}

void UnloadVulkanLibrary()
{
  if ((--vulkan_module_ref_count) > 0)
    return;

  ResetVulkanLibraryFunctionPointers();
  FreeLibrary(vulkan_module);
  vulkan_module = nullptr;
}

#elif defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_XCB_KHR) ||                     \
    defined(VK_USE_PLATFORM_ANDROID_KHR)

static void* vulkan_module;
static std::atomic_int vulkan_module_ref_count = {0};

bool LoadVulkanLibrary()
{
  // Not thread safe if a second thread calls the loader whilst the first is still in-progress.
  if (vulkan_module)
  {
    vulkan_module_ref_count++;
    return true;
  }

  // Names of libraries to search. Desktop should use libvulkan.so.1 or libvulkan.so.
  static const char* search_lib_names[] = {"libvulkan.so.1", "libvulkan.so"};

  for (size_t i = 0; i < ArraySize(search_lib_names); i++)
  {
    vulkan_module = dlopen(search_lib_names[i], RTLD_NOW);
    if (vulkan_module)
      break;
  }

  if (!vulkan_module)
  {
    ERROR_LOG(VIDEO, "Failed to load or locate libvulkan.so");
    return false;
  }

  bool required_functions_missing = false;
  auto LoadFunction = [&](void** func_ptr, const char* name, bool is_required) {
    *func_ptr = dlsym(vulkan_module, name);
    if (!(*func_ptr) && is_required)
    {
      ERROR_LOG(VIDEO, "Vulkan: Failed to load required module function %s", name);
      required_functions_missing = true;
    }
  };

#define VULKAN_MODULE_ENTRY_POINT(name, required)                                                  \
  LoadFunction(reinterpret_cast<void**>(&name), #name, required);
#include "VideoBackends/Vulkan/VulkanEntryPoints.inl"
#undef VULKAN_MODULE_ENTRY_POINT

  if (required_functions_missing)
  {
    ResetVulkanLibraryFunctionPointers();
    dlclose(vulkan_module);
    vulkan_module = nullptr;
    return false;
  }

  vulkan_module_ref_count++;
  return true;
}

void UnloadVulkanLibrary()
{
  if ((--vulkan_module_ref_count) > 0)
    return;

  ResetVulkanLibraryFunctionPointers();
  dlclose(vulkan_module);
  vulkan_module = nullptr;
}
#else

//#warning Unknown platform, not compiling loader.

bool LoadVulkanLibrary()
{
  return false;
}

void UnloadVulkanLibrary()
{
  ResetVulkanLibraryFunctionPointers();
}

#endif

bool LoadVulkanInstanceFunctions(VkInstance instance)
{
  bool required_functions_missing = false;
  auto LoadFunction = [&](PFN_vkVoidFunction* func_ptr, const char* name, bool is_required) {
    *func_ptr = vkGetInstanceProcAddr(instance, name);
    if (!(*func_ptr) && is_required)
    {
      ERROR_LOG(VIDEO, "Vulkan: Failed to load required instance function %s", name);
      required_functions_missing = true;
    }
  };

#define VULKAN_INSTANCE_ENTRY_POINT(name, required)                                                \
  LoadFunction(reinterpret_cast<PFN_vkVoidFunction*>(&name), #name, required);
#include "VideoBackends/Vulkan/VulkanEntryPoints.inl"
#undef VULKAN_INSTANCE_ENTRY_POINT

  return !required_functions_missing;
}

bool LoadVulkanDeviceFunctions(VkDevice device)
{
  bool required_functions_missing = false;
  auto LoadFunction = [&](PFN_vkVoidFunction* func_ptr, const char* name, bool is_required) {
    *func_ptr = vkGetDeviceProcAddr(device, name);
    if (!(*func_ptr) && is_required)
    {
      ERROR_LOG(VIDEO, "Vulkan: Failed to load required device function %s", name);
      required_functions_missing = true;
    }
  };

#define VULKAN_DEVICE_ENTRY_POINT(name, required)                                                  \
  LoadFunction(reinterpret_cast<PFN_vkVoidFunction*>(&name), #name, required);
#include "VideoBackends/Vulkan/VulkanEntryPoints.inl"
#undef VULKAN_DEVICE_ENTRY_POINT

  return !required_functions_missing;
}

const char* VkResultToString(VkResult res)
{
  switch (res)
  {
  case VK_SUCCESS:
    return "VK_SUCCESS";

  case VK_NOT_READY:
    return "VK_NOT_READY";

  case VK_TIMEOUT:
    return "VK_TIMEOUT";

  case VK_EVENT_SET:
    return "VK_EVENT_SET";

  case VK_EVENT_RESET:
    return "VK_EVENT_RESET";

  case VK_INCOMPLETE:
    return "VK_INCOMPLETE";

  case VK_ERROR_OUT_OF_HOST_MEMORY:
    return "VK_ERROR_OUT_OF_HOST_MEMORY";

  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    return "VK_ERROR_OUT_OF_DEVICE_MEMORY";

  case VK_ERROR_INITIALIZATION_FAILED:
    return "VK_ERROR_INITIALIZATION_FAILED";

  case VK_ERROR_DEVICE_LOST:
    return "VK_ERROR_DEVICE_LOST";

  case VK_ERROR_MEMORY_MAP_FAILED:
    return "VK_ERROR_MEMORY_MAP_FAILED";

  case VK_ERROR_LAYER_NOT_PRESENT:
    return "VK_ERROR_LAYER_NOT_PRESENT";

  case VK_ERROR_EXTENSION_NOT_PRESENT:
    return "VK_ERROR_EXTENSION_NOT_PRESENT";

  case VK_ERROR_FEATURE_NOT_PRESENT:
    return "VK_ERROR_FEATURE_NOT_PRESENT";

  case VK_ERROR_INCOMPATIBLE_DRIVER:
    return "VK_ERROR_INCOMPATIBLE_DRIVER";

  case VK_ERROR_TOO_MANY_OBJECTS:
    return "VK_ERROR_TOO_MANY_OBJECTS";

  case VK_ERROR_FORMAT_NOT_SUPPORTED:
    return "VK_ERROR_FORMAT_NOT_SUPPORTED";

  case VK_ERROR_SURFACE_LOST_KHR:
    return "VK_ERROR_SURFACE_LOST_KHR";

  case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
    return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";

  case VK_SUBOPTIMAL_KHR:
    return "VK_SUBOPTIMAL_KHR";

  case VK_ERROR_OUT_OF_DATE_KHR:
    return "VK_ERROR_OUT_OF_DATE_KHR";

  case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
    return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";

  case VK_ERROR_VALIDATION_FAILED_EXT:
    return "VK_ERROR_VALIDATION_FAILED_EXT";

  case VK_ERROR_INVALID_SHADER_NV:
    return "VK_ERROR_INVALID_SHADER_NV";

  default:
    return "UNKNOWN_VK_RESULT";
  }
}

void LogVulkanResult(int level, const char* func_name, VkResult res, const char* msg, ...)
{
  std::va_list ap;
  va_start(ap, msg);
  std::string real_msg = StringFromFormatV(msg, ap);
  va_end(ap);

  real_msg = StringFromFormat("(%s) %s (%d: %s)", func_name, real_msg.c_str(),
                              static_cast<int>(res), VkResultToString(res));

  GENERIC_LOG(LogTypes::VIDEO, static_cast<LogTypes::LOG_LEVELS>(level), "%s", real_msg.c_str());
}

}  // namespace Vulkan
