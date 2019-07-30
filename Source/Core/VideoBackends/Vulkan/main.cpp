// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <vector>

#include "Common/Logging/LogManager.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/Constants.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/PerfQuery.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/SwapChain.h"
#include "VideoBackends/Vulkan/VertexManager.h"
#include "VideoBackends/Vulkan/VideoBackend.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

#if defined(VK_USE_PLATFORM_MACOS_MVK)
#include <objc/message.h>
#endif

namespace Vulkan
{
void VideoBackend::InitBackendInfo()
{
  VulkanContext::PopulateBackendInfo(&g_Config);

  if (LoadVulkanLibrary())
  {
    VkInstance temp_instance =
        VulkanContext::CreateVulkanInstance(WindowSystemType::Headless, false, false);
    if (temp_instance)
    {
      if (LoadVulkanInstanceFunctions(temp_instance))
      {
        VulkanContext::GPUList gpu_list = VulkanContext::EnumerateGPUs(temp_instance);
        VulkanContext::PopulateBackendInfoAdapters(&g_Config, gpu_list);

        if (!gpu_list.empty())
        {
          // Use the selected adapter, or the first to fill features.
          size_t device_index = static_cast<size_t>(g_Config.iAdapter);
          if (device_index >= gpu_list.size())
            device_index = 0;

          VkPhysicalDevice gpu = gpu_list[device_index];
          VkPhysicalDeviceProperties properties;
          vkGetPhysicalDeviceProperties(gpu, &properties);
          VkPhysicalDeviceFeatures features;
          vkGetPhysicalDeviceFeatures(gpu, &features);
          VulkanContext::PopulateBackendInfoFeatures(&g_Config, gpu, properties, features);
          VulkanContext::PopulateBackendInfoMultisampleModes(&g_Config, gpu, properties);
        }
      }

      vkDestroyInstance(temp_instance, nullptr);
    }
    else
    {
      PanicAlert("Failed to create Vulkan instance.");
    }

    UnloadVulkanLibrary();
  }
  else
  {
    PanicAlert("Failed to load Vulkan library.");
  }
}

// Helper method to check whether the Host GPU logging category is enabled.
static bool IsHostGPULoggingEnabled()
{
  return LogManager::GetInstance()->IsEnabled(LogTypes::HOST_GPU, LogTypes::LERROR);
}

// Helper method to determine whether to enable the debug report extension.
static bool ShouldEnableDebugReports(bool enable_validation_layers)
{
  // Enable debug reports if the Host GPU log option is checked, or validation layers are enabled.
  // The only issue here is that if Host GPU is not checked when the instance is created, the debug
  // report extension will not be enabled, requiring the game to be restarted before any reports
  // will be logged. Otherwise, we'd have to enable debug reports on every instance, when most
  // users will never check the Host GPU logging category.
  return enable_validation_layers || IsHostGPULoggingEnabled();
}

bool VideoBackend::Initialize(const WindowSystemInfo& wsi)
{
  if (!LoadVulkanLibrary())
  {
    PanicAlert("Failed to load Vulkan library.");
    return false;
  }

  // Check for presence of the validation layers before trying to enable it
  bool enable_validation_layer = g_Config.bEnableValidationLayer;
  if (enable_validation_layer && !VulkanContext::CheckValidationLayerAvailablility())
  {
    WARN_LOG(VIDEO, "Validation layer requested but not available, disabling.");
    enable_validation_layer = false;
  }

  // Create Vulkan instance, needed before we can create a surface, or enumerate devices.
  // We use this instance to fill in backend info, then re-use it for the actual device.
  bool enable_surface = wsi.type != WindowSystemType::Headless;
  bool enable_debug_reports = ShouldEnableDebugReports(enable_validation_layer);
  VkInstance instance =
      VulkanContext::CreateVulkanInstance(wsi.type, enable_debug_reports, enable_validation_layer);
  if (instance == VK_NULL_HANDLE)
  {
    PanicAlert("Failed to create Vulkan instance.");
    UnloadVulkanLibrary();
    return false;
  }

  // Load instance function pointers.
  if (!LoadVulkanInstanceFunctions(instance))
  {
    PanicAlert("Failed to load Vulkan instance functions.");
    vkDestroyInstance(instance, nullptr);
    UnloadVulkanLibrary();
    return false;
  }

  // Obtain a list of physical devices (GPUs) from the instance.
  // We'll re-use this list later when creating the device.
  VulkanContext::GPUList gpu_list = VulkanContext::EnumerateGPUs(instance);
  if (gpu_list.empty())
  {
    PanicAlert("No Vulkan physical devices available.");
    vkDestroyInstance(instance, nullptr);
    UnloadVulkanLibrary();
    return false;
  }

  // Populate BackendInfo with as much information as we can at this point.
  VulkanContext::PopulateBackendInfo(&g_Config);
  VulkanContext::PopulateBackendInfoAdapters(&g_Config, gpu_list);

  // We need the surface before we can create a device, as some parameters depend on it.
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  if (enable_surface)
  {
    surface = SwapChain::CreateVulkanSurface(instance, wsi);
    if (surface == VK_NULL_HANDLE)
    {
      PanicAlert("Failed to create Vulkan surface.");
      vkDestroyInstance(instance, nullptr);
      UnloadVulkanLibrary();
      return false;
    }
  }

  // Since we haven't called InitializeShared yet, iAdapter may be out of range,
  // so we have to check it ourselves.
  size_t selected_adapter_index = static_cast<size_t>(g_Config.iAdapter);
  if (selected_adapter_index >= gpu_list.size())
  {
    WARN_LOG(VIDEO, "Vulkan adapter index out of range, selecting first adapter.");
    selected_adapter_index = 0;
  }

  // Now we can create the Vulkan device. VulkanContext takes ownership of the instance and surface.
  g_vulkan_context = VulkanContext::Create(instance, gpu_list[selected_adapter_index], surface,
                                           enable_debug_reports, enable_validation_layer);
  if (!g_vulkan_context)
  {
    PanicAlert("Failed to create Vulkan device");
    UnloadVulkanLibrary();
    return false;
  }

  // Since VulkanContext maintains a copy of the device features and properties, we can use this
  // to initialize the backend information, so that we don't need to enumerate everything again.
  VulkanContext::PopulateBackendInfoFeatures(&g_Config, g_vulkan_context->GetPhysicalDevice(),
                                             g_vulkan_context->GetDeviceProperties(),
                                             g_vulkan_context->GetDeviceFeatures());
  VulkanContext::PopulateBackendInfoMultisampleModes(
      &g_Config, g_vulkan_context->GetPhysicalDevice(), g_vulkan_context->GetDeviceProperties());

  // With the backend information populated, we can now initialize videocommon.
  InitializeShared();

  // Create command buffers. We do this separately because the other classes depend on it.
  g_command_buffer_mgr = std::make_unique<CommandBufferManager>(g_Config.bBackendMultithreading);
  if (!g_command_buffer_mgr->Initialize())
  {
    PanicAlert("Failed to create Vulkan command buffers");
    Shutdown();
    return false;
  }

  // Remaining classes are also dependent on object cache.
  g_object_cache = std::make_unique<ObjectCache>();
  if (!g_object_cache->Initialize())
  {
    PanicAlert("Failed to initialize Vulkan object cache.");
    Shutdown();
    return false;
  }

  // Create swap chain. This has to be done early so that the target size is correct for auto-scale.
  std::unique_ptr<SwapChain> swap_chain;
  if (surface != VK_NULL_HANDLE)
  {
    swap_chain = SwapChain::Create(wsi, surface, g_ActiveConfig.bVSyncActive);
    if (!swap_chain)
    {
      PanicAlert("Failed to create Vulkan swap chain.");
      Shutdown();
      return false;
    }
  }

  if (!StateTracker::CreateInstance())
  {
    PanicAlert("Failed to create state tracker");
    Shutdown();
    return false;
  }

  // Create main wrapper instances.
  g_renderer = std::make_unique<Renderer>(std::move(swap_chain), wsi.render_surface_scale);
  g_vertex_manager = std::make_unique<VertexManager>();
  g_shader_cache = std::make_unique<VideoCommon::ShaderCache>();
  g_framebuffer_manager = std::make_unique<FramebufferManager>();
  g_texture_cache = std::make_unique<TextureCacheBase>();
  g_perf_query = std::make_unique<PerfQuery>();

  if (!g_vertex_manager->Initialize() || !g_shader_cache->Initialize() ||
      !g_renderer->Initialize() || !g_framebuffer_manager->Initialize() ||
      !g_texture_cache->Initialize() || !PerfQuery::GetInstance()->Initialize())
  {
    PanicAlert("Failed to initialize renderer classes");
    Shutdown();
    return false;
  }

  g_shader_cache->InitializeShaderCache();
  return true;
}

void VideoBackend::Shutdown()
{
  if (g_vulkan_context)
    vkDeviceWaitIdle(g_vulkan_context->GetDevice());

  if (g_shader_cache)
    g_shader_cache->Shutdown();

  if (g_object_cache)
    g_object_cache->Shutdown();

  if (g_renderer)
    g_renderer->Shutdown();

  g_perf_query.reset();
  g_texture_cache.reset();
  g_framebuffer_manager.reset();
  g_shader_cache.reset();
  g_vertex_manager.reset();
  g_renderer.reset();
  g_object_cache.reset();
  StateTracker::DestroyInstance();
  g_command_buffer_mgr.reset();
  g_vulkan_context.reset();
  ShutdownShared();
  UnloadVulkanLibrary();
}

#if defined(VK_USE_PLATFORM_MACOS_MVK)
static bool IsRunningOnMojaveOrHigher()
{
  // id processInfo = [NSProcessInfo processInfo]
  id processInfo = reinterpret_cast<id (*)(Class, SEL)>(objc_msgSend)(
      objc_getClass("NSProcessInfo"), sel_getUid("processInfo"));
  if (!processInfo)
    return false;

  struct OSVersion  // NSOperatingSystemVersion
  {
    size_t major_version;  // NSInteger majorVersion
    size_t minor_version;  // NSInteger minorVersion
    size_t patch_version;  // NSInteger patchVersion
  };

  // const bool meets_requirement = [processInfo isOperatingSystemAtLeastVersion:required_version];
  constexpr OSVersion required_version = {10, 14, 0};
  const bool meets_requirement = reinterpret_cast<bool (*)(id, SEL, OSVersion)>(objc_msgSend)(
      processInfo, sel_getUid("isOperatingSystemAtLeastVersion:"), required_version);
  return meets_requirement;
}
#endif

void VideoBackend::PrepareWindow(const WindowSystemInfo& wsi)
{
#if defined(VK_USE_PLATFORM_MACOS_MVK)
  // This is kinda messy, but it avoids having to write Objective C++ just to create a metal layer.
  id view = reinterpret_cast<id>(wsi.render_surface);
  Class clsCAMetalLayer = objc_getClass("CAMetalLayer");
  if (!clsCAMetalLayer)
  {
    ERROR_LOG(VIDEO, "Failed to get CAMetalLayer class.");
    return;
  }

  // [CAMetalLayer layer]
  id layer = reinterpret_cast<id (*)(Class, SEL)>(objc_msgSend)(objc_getClass("CAMetalLayer"),
                                                                sel_getUid("layer"));
  if (!layer)
  {
    ERROR_LOG(VIDEO, "Failed to create Metal layer.");
    return;
  }

  // [view setWantsLayer:YES]
  reinterpret_cast<void (*)(id, SEL, BOOL)>(objc_msgSend)(view, sel_getUid("setWantsLayer:"), YES);

  // [view setLayer:layer]
  reinterpret_cast<void (*)(id, SEL, id)>(objc_msgSend)(view, sel_getUid("setLayer:"), layer);

  // NSScreen* screen = [NSScreen mainScreen]
  id screen = reinterpret_cast<id (*)(Class, SEL)>(objc_msgSend)(objc_getClass("NSScreen"),
                                                                 sel_getUid("mainScreen"));

  // CGFloat factor = [screen backingScaleFactor]
  double factor =
      reinterpret_cast<double (*)(id, SEL)>(objc_msgSend)(screen, sel_getUid("backingScaleFactor"));

  // layer.contentsScale = factor
  reinterpret_cast<void (*)(id, SEL, double)>(objc_msgSend)(layer, sel_getUid("setContentsScale:"),
                                                            factor);
  // The Metal version included with MacOS 10.13 and below does not support several features we
  // require. Furthermore, the drivers seem to choke on our shaders (mainly Intel). So, we warn
  // the user that this is an unsupported configuration, but permit them to continue.
  if (!IsRunningOnMojaveOrHigher())
  {
    PanicAlertT(
        "You are attempting to use the Vulkan (Metal) backend on an unsupported operating system. "
        "For all functionality to be enabled, you must use macOS 10.14 (Mojave) or newer. Please "
        "do not report any issues encountered unless they also occur on 10.14+.");
  }
#endif
}
}  // namespace Vulkan
