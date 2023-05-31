// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#define VMA_IMPLEMENTATION
#include "VideoBackends/Vulkan/VulkanLoader.h"

#include <atomic>
#include <cstdarg>
#include <cstdlib>

#if defined(ANDROID)
#include <adrenotools/driver.h>
#include <dlfcn.h>
#endif

#include "Common/CommonFuncs.h"
#include "Common/DynamicLibrary.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "VideoCommon/VideoConfig.h"

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

static Common::DynamicLibrary s_vulkan_module;

static bool OpenVulkanLibrary(bool force_system_library)
{
#if defined(__APPLE__)
  // Check if a path to a specific Vulkan library has been specified.
  char* libvulkan_env = getenv("LIBVULKAN_PATH");
  if (libvulkan_env && s_vulkan_module.Open(libvulkan_env))
    return true;

  // Use the libMoltenVK.dylib from the application bundle.
  std::string filename = File::GetBundleDirectory() + "/Contents/Frameworks/libMoltenVK.dylib";
  return s_vulkan_module.Open(filename.c_str());
#else

#if defined(ANDROID) && _M_ARM_64
  const std::string& driver_lib_name = g_Config.customDriverLibraryName;

  if (!force_system_library && !driver_lib_name.empty() && SupportsCustomDriver())
  {
    std::string tmp_dir = File::GetGpuDriverDirectory(D_GPU_DRIVERS_TMP);
    std::string hook_dir = File::GetGpuDriverDirectory(D_GPU_DRIVERS_HOOKS);
    std::string file_redirect_dir = File::GetGpuDriverDirectory(D_GPU_DRIVERS_FILE_REDIRECT);
    std::string driver_dir = File::GetGpuDriverDirectory(D_GPU_DRIVERS_EXTRACTED);
    INFO_LOG_FMT(HOST_GPU, "Loading driver: {}", driver_lib_name);

    s_vulkan_module = adrenotools_open_libvulkan(
        RTLD_NOW, ADRENOTOOLS_DRIVER_FILE_REDIRECT | ADRENOTOOLS_DRIVER_CUSTOM, tmp_dir.c_str(),
        hook_dir.c_str(), driver_dir.c_str(), driver_lib_name.c_str(), file_redirect_dir.c_str(),
        nullptr);
    if (s_vulkan_module.IsOpen())
    {
      INFO_LOG_FMT(HOST_GPU, "Successfully loaded driver: {}", driver_lib_name);
      return true;
    }
    else
    {
      WARN_LOG_FMT(HOST_GPU, "Loading driver {} failed.", driver_lib_name);
    }
  }
#endif

  WARN_LOG_FMT(HOST_GPU, "Loading system driver");
  std::string filename = Common::DynamicLibrary::GetVersionedFilename("vulkan", 1);
  if (s_vulkan_module.Open(filename.c_str()))
    return true;

  // Android devices may not have libvulkan.so.1, only libvulkan.so.
  filename = Common::DynamicLibrary::GetVersionedFilename("vulkan");
  return s_vulkan_module.Open(filename.c_str());
#endif
}

bool LoadVulkanLibrary(bool force_system_library)
{
  if (!s_vulkan_module.IsOpen() && !OpenVulkanLibrary(force_system_library))
    return false;

#define VULKAN_MODULE_ENTRY_POINT(name, required)                                                  \
  if (!s_vulkan_module.GetSymbol(#name, &name) && required)                                        \
  {                                                                                                \
    ERROR_LOG_FMT(VIDEO, "Vulkan: Failed to load required module function {}", #name);             \
    ResetVulkanLibraryFunctionPointers();                                                          \
    s_vulkan_module.Close();                                                                       \
    return false;                                                                                  \
  }
#include "VideoBackends/Vulkan/VulkanEntryPoints.inl"
#undef VULKAN_MODULE_ENTRY_POINT

  return true;
}

void UnloadVulkanLibrary()
{
  s_vulkan_module.Close();
  if (!s_vulkan_module.IsOpen())
    ResetVulkanLibraryFunctionPointers();
}

bool LoadVulkanInstanceFunctions(VkInstance instance)
{
  bool required_functions_missing = false;
  auto LoadFunction = [&](PFN_vkVoidFunction* func_ptr, const char* name, bool is_required) {
    *func_ptr = vkGetInstanceProcAddr(instance, name);
    if (!(*func_ptr) && is_required)
    {
      ERROR_LOG_FMT(HOST_GPU, "Vulkan: Failed to load required instance function {}", name);
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
      ERROR_LOG_FMT(HOST_GPU, "Vulkan: Failed to load required device function {}", name);
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

void LogVulkanResult(Common::Log::LogLevel level, const char* func_name, VkResult res,
                     const char* msg)
{
  GENERIC_LOG_FMT(Common::Log::LogType::VIDEO, level, "({}) {} ({}: {})", func_name, msg,
                  static_cast<int>(res), VkResultToString(res));
}

#ifdef ANDROID
static bool CheckKgslPresent()
{
  constexpr auto KgslPath{"/dev/kgsl-3d0"};

  return access(KgslPath, F_OK) == 0;
}

bool SupportsCustomDriver()
{
  return android_get_device_api_level() >= 28 && CheckKgslPresent();
}
#endif

}  // namespace Vulkan
