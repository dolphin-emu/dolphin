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
#define LOADER_WINDOWS 1

#elif defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_XCB_KHR) ||                     \
    defined(VK_USE_PLATFORM_ANDROID_KHR) || defined(USE_HEADLESS)
#include <dlfcn.h>
#define LOADER_UNIX 1
#endif

namespace
{
#if defined(LOADER_WINDOWS)
using Module = HMODULE;
const char* const search_lib_names[] = {"vulkan-1.dll"};

Module LoadModule(const char* lib_name) noexcept
{
  return LoadLibraryA(lib_name);
}

void UnloadModule(Module* module) noexcept
{
  FreeLibrary(*module);
  *module = nullptr;
}

void* LoadModuleFunction(Module module, const char* name) noexcept
{
  return static_cast<void*>(GetProcAddress(module, name));
}

#elif defined(LOADER_UNIX)
using Module = void*;
const char* const search_lib_names[] = {"libvulkan.so.1", "libvulkan.so"};

Module LoadModule(const char* lib_name) noexcept
{
  return dlopen(lib_name, RTLD_NOW);
}

void UnloadModule(Module* module) noexcept
{
  dlclose(*module);
  *module = nullptr;
}

void* LoadModuleFunction(Module module, const char* name) noexcept
{
  return dlsym(module, name);
}

#else
using Module = void*;
const char* const search_lib_names[] = {};

Module LoadModule(const char* lib_name) noexcept
{
  return nullptr;
}

void UnloadModule(Module* module) noexcept
{
}

void* LoadModuleFunction(Module module, const char* name) noexcept
{
  return nullptr;
}

#endif
}  // namespace

#define VULKAN_ENTRY_POINT(name, required) PFN_##name name;
VULKAN_EACH_ENTRY_POINT(VULKAN_ENTRY_POINT)
#undef VULKAN_ENTRY_POINT

namespace Vulkan
{
static void ResetVulkanLibraryFunctionPointers()
{
#define VULKAN_ENTRY_POINT(name, required) name = nullptr;
  VULKAN_EACH_ENTRY_POINT(VULKAN_ENTRY_POINT)
#undef VULKAN_ENTRY_POINT
}

static Module vulkan_module = {};
static std::atomic_int vulkan_module_ref_count = {0};

bool LoadVulkanLibrary()
{
  // Not thread safe if a second thread calls the loader whilst the first is still in-progress.
  if (vulkan_module)
  {
    vulkan_module_ref_count++;
    return true;
  }

  for (auto lib_name : search_lib_names)
  {
    vulkan_module = LoadModule(lib_name);
    if (vulkan_module)
      break;
  }

  if (!vulkan_module)
  {
    ERROR_LOG(VIDEO, "Failed to load or locate libvulkan");
    return false;
  }

  bool required_functions_missing = false;
  auto LoadFunction = [&](const char* name, bool is_required) {
    void* func_ptr = LoadModuleFunction(vulkan_module, name);
    if (!func_ptr && is_required)
    {
      ERROR_LOG(VIDEO, "Vulkan: Failed to load required module function %s", name);
      required_functions_missing = true;
    }
    return func_ptr;
  };

#define VULKAN_MODULE_ENTRY_POINT(name, required)                                                  \
  *reinterpret_cast<void**>(&name) = LoadFunction(#name, required);
  VULKAN_EACH_MODULE_ENTRY_POINT(VULKAN_MODULE_ENTRY_POINT)
#undef VULKAN_MODULE_ENTRY_POINT

  if (required_functions_missing)
  {
    ResetVulkanLibraryFunctionPointers();
    UnloadModule(&vulkan_module);
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
  UnloadModule(&vulkan_module);
}

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
  VULKAN_EACH_INSTANCE_ENTRY_POINT(VULKAN_INSTANCE_ENTRY_POINT)
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
  VULKAN_EACH_DEVICE_ENTRY_POINT(VULKAN_DEVICE_ENTRY_POINT)
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
