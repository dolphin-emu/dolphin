// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Vulkan/VKSwapChain.h"

#include <algorithm>
#include <cstdint>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/VKTexture.h"
#include "VideoBackends/Vulkan/VulkanContext.h"
#include "VideoCommon/Present.h"

#if defined(VK_USE_PLATFORM_XLIB_KHR)
#include <X11/Xlib.h>
#endif

namespace Vulkan
{
SwapChain::SwapChain(const WindowSystemInfo& wsi, VkSurfaceKHR surface, bool vsync)
    : m_wsi(wsi), m_surface(surface), m_vsync_enabled(vsync),
      m_fullscreen_supported(g_vulkan_context->SupportsExclusiveFullscreen(wsi, surface))
{
}

SwapChain::~SwapChain()
{
  DestroySwapChainImages();
  DestroySwapChain();
  DestroySurface();
}

VkSurfaceKHR SwapChain::CreateVulkanSurface(VkInstance instance, const WindowSystemInfo& wsi)
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
  if (wsi.type == WindowSystemType::Windows)
  {
    VkWin32SurfaceCreateInfoKHR surface_create_info = {
        VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,  // VkStructureType               sType
        nullptr,                                          // const void*                   pNext
        0,                                                // VkWin32SurfaceCreateFlagsKHR  flags
        nullptr,                                          // HINSTANCE                     hinstance
        reinterpret_cast<HWND>(wsi.render_surface)        // HWND                          hwnd
    };

    VkSurfaceKHR surface;
    VkResult res = vkCreateWin32SurfaceKHR(instance, &surface_create_info, nullptr, &surface);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkCreateWin32SurfaceKHR failed: ");
      return VK_NULL_HANDLE;
    }

    return surface;
  }
#endif

#if defined(VK_USE_PLATFORM_XLIB_KHR)
  if (wsi.type == WindowSystemType::X11)
  {
    VkXlibSurfaceCreateInfoKHR surface_create_info = {
        VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,  // VkStructureType               sType
        nullptr,                                         // const void*                   pNext
        0,                                               // VkXlibSurfaceCreateFlagsKHR   flags
        static_cast<Display*>(wsi.display_connection),   // Display*                      dpy
        reinterpret_cast<Window>(wsi.render_surface)     // Window                        window
    };

    VkSurfaceKHR surface;
    VkResult res = vkCreateXlibSurfaceKHR(instance, &surface_create_info, nullptr, &surface);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkCreateXlibSurfaceKHR failed: ");
      return VK_NULL_HANDLE;
    }

    return surface;
  }
#endif

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
  if (wsi.type == WindowSystemType::Android)
  {
    VkAndroidSurfaceCreateInfoKHR surface_create_info = {
        VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,  // VkStructureType                sType
        nullptr,                                            // const void*                    pNext
        0,                                                  // VkAndroidSurfaceCreateFlagsKHR flags
        reinterpret_cast<ANativeWindow*>(wsi.render_surface)  // ANativeWindow* window
    };

    VkSurfaceKHR surface;
    VkResult res = vkCreateAndroidSurfaceKHR(instance, &surface_create_info, nullptr, &surface);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkCreateAndroidSurfaceKHR failed: ");
      return VK_NULL_HANDLE;
    }

    return surface;
  }
#endif

#if defined(VK_USE_PLATFORM_METAL_EXT)
  if (wsi.type == WindowSystemType::MacOS)
  {
    VkMetalSurfaceCreateInfoEXT surface_create_info = {
        VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT, nullptr, 0,
        static_cast<const CAMetalLayer*>(wsi.render_surface)};

    VkSurfaceKHR surface;
    VkResult res = vkCreateMetalSurfaceEXT(instance, &surface_create_info, nullptr, &surface);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkCreateMetalSurfaceEXT failed: ");
      return VK_NULL_HANDLE;
    }

    return surface;
  }
#endif

  return VK_NULL_HANDLE;
}

std::unique_ptr<SwapChain> SwapChain::Create(const WindowSystemInfo& wsi, VkSurfaceKHR surface,
                                             bool vsync)
{
  std::unique_ptr<SwapChain> swap_chain = std::make_unique<SwapChain>(wsi, surface, vsync);
  if (!swap_chain->CreateSwapChain() || !swap_chain->SetupSwapChainImages())
    return nullptr;

  return swap_chain;
}

bool SwapChain::SelectSurfaceFormat()
{
  u32 format_count;
  VkResult res = vkGetPhysicalDeviceSurfaceFormatsKHR(g_vulkan_context->GetPhysicalDevice(),
                                                      m_surface, &format_count, nullptr);
  if (res != VK_SUCCESS || format_count == 0)
  {
    LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceSurfaceFormatsKHR failed: ");
    return false;
  }

  std::vector<VkSurfaceFormatKHR> surface_formats(format_count);
  res = vkGetPhysicalDeviceSurfaceFormatsKHR(g_vulkan_context->GetPhysicalDevice(), m_surface,
                                             &format_count, surface_formats.data());
  ASSERT(res == VK_SUCCESS);

  // If there is a single undefined surface format, the device doesn't care, so we'll just use RGBA8
  if (surface_formats[0].format == VK_FORMAT_UNDEFINED)
  {
    m_surface_format.format = VK_FORMAT_R8G8B8A8_UNORM;
    m_surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    return true;
  }

  const VkSurfaceFormatKHR* surface_format_RGBA8 = nullptr;
  const VkSurfaceFormatKHR* surface_format_BGRA8 = nullptr;
  const VkSurfaceFormatKHR* surface_format_RGB10_A2 = nullptr;
  const VkSurfaceFormatKHR* surface_format_RGBA16F_scRGB = nullptr;

  // Try to find all suitable formats.
  for (const VkSurfaceFormatKHR& surface_format : surface_formats)
  {
    // Some drivers seem to return a RGBA8 SRGB format here (Intel Mesa).
    // Some other drivers return both a RGBA8 SRGB and UNORM formats (Nvidia).
    // This results in gamma correction when presenting to the screen, which we don't want,
    // because we already apply gamma ourselves, and we might not use sRGB gamma.
    // Force using a linear format instead, if this is the case.
    VkFormat format = VKTexture::GetLinearFormat(surface_format.format);
    if (format == VK_FORMAT_R8G8B8A8_UNORM)
      surface_format_RGBA8 = &surface_format;
    else if (format == VK_FORMAT_B8G8R8A8_UNORM)
      surface_format_BGRA8 = &surface_format;
    else if (format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 &&
             surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      surface_format_RGB10_A2 = &surface_format;
    else if (format == VK_FORMAT_R16G16B16A16_SFLOAT &&
             surface_format.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT)
      surface_format_RGBA16F_scRGB = &surface_format;
    else
      continue;
  }

  const VkSurfaceFormatKHR* surface_format = nullptr;

  // Pick the best format.
  // "g_ActiveConfig" might not have been been updated yet.
  if (g_Config.bHDR && surface_format_RGBA16F_scRGB)
    surface_format = surface_format_RGBA16F_scRGB;
  else if (surface_format_RGB10_A2)
    surface_format = surface_format_RGB10_A2;
  else if (surface_format_RGBA8)
    surface_format = surface_format_RGBA8;
  else if (surface_format_BGRA8)
    surface_format = surface_format_BGRA8;

  if (surface_format)
  {
    const VkFormat format = VKTexture::GetLinearFormat(surface_format->format);
    if (format == VK_FORMAT_R8G8B8A8_UNORM)
      m_texture_format = AbstractTextureFormat::RGBA8;
    else if (format == VK_FORMAT_B8G8R8A8_UNORM)
      m_texture_format = AbstractTextureFormat::BGRA8;
    else if (format == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
      m_texture_format = AbstractTextureFormat::RGB10_A2;
    else if (format == VK_FORMAT_R16G16B16A16_SFLOAT)
      m_texture_format = AbstractTextureFormat::RGBA16F;

    m_surface_format.format = format;
    m_surface_format.colorSpace = surface_format->colorSpace;
    return true;
  }

  PanicAlertFmt("Failed to find a suitable format for swap chain buffers.");
  return false;
}

bool SwapChain::SelectPresentMode()
{
  VkResult res;
  u32 mode_count;
  res = vkGetPhysicalDeviceSurfacePresentModesKHR(g_vulkan_context->GetPhysicalDevice(), m_surface,
                                                  &mode_count, nullptr);
  if (res != VK_SUCCESS || mode_count == 0)
  {
    LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceSurfaceFormatsKHR failed: ");
    return false;
  }

  std::vector<VkPresentModeKHR> present_modes(mode_count);
  res = vkGetPhysicalDeviceSurfacePresentModesKHR(g_vulkan_context->GetPhysicalDevice(), m_surface,
                                                  &mode_count, present_modes.data());
  ASSERT(res == VK_SUCCESS);

  // Checks if a particular mode is supported, if it is, returns that mode.
  auto CheckForMode = [&present_modes](VkPresentModeKHR check_mode) {
    auto it = std::ranges::find_if(
        present_modes, [check_mode](VkPresentModeKHR mode) { return check_mode == mode; });
    return it != present_modes.end();
  };

  // If vsync is enabled, use VK_PRESENT_MODE_FIFO_KHR.
  // This check should not fail with conforming drivers, as the FIFO present mode is mandated by
  // the specification (VK_KHR_swapchain). In case it isn't though, fall through to any other mode.
  if (m_vsync_enabled && CheckForMode(VK_PRESENT_MODE_FIFO_KHR))
  {
    m_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    return true;
  }

  // Prefer screen-tearing, if possible, for lowest latency.
  if (CheckForMode(VK_PRESENT_MODE_IMMEDIATE_KHR))
  {
    m_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    return true;
  }

  // Use optimized-vsync above vsync.
  if (CheckForMode(VK_PRESENT_MODE_MAILBOX_KHR))
  {
    m_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
    return true;
  }

  // Fall back to whatever is available.
  m_present_mode = present_modes[0];
  return true;
}

bool SwapChain::CreateSwapChain()
{
  // Look up surface properties to determine image count and dimensions
  VkSurfaceCapabilitiesKHR surface_capabilities;
  VkResult res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_vulkan_context->GetPhysicalDevice(),
                                                           m_surface, &surface_capabilities);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed: ");
    return false;
  }

  // Select swap chain format and present mode
  if (!SelectSurfaceFormat() || !SelectPresentMode())
    return false;

  // Select number of images in swap chain, we prefer one buffer in the background to work on
  uint32_t image_count = surface_capabilities.minImageCount + 1;

  // maxImageCount can be zero, in which case there isn't an upper limit on the number of buffers.
  if (surface_capabilities.maxImageCount > 0)
    image_count = std::min(image_count, surface_capabilities.maxImageCount);

  // Determine the dimensions of the swap chain. Values of -1 indicate the size we specify here
  // determines window size?
  VkExtent2D size = surface_capabilities.currentExtent;
  if (size.width == UINT32_MAX)
  {
    size.width = std::max(g_presenter->GetBackbufferWidth(), 1);
    size.height = std::max(g_presenter->GetBackbufferHeight(), 1);
  }
  size.width = std::clamp(size.width, surface_capabilities.minImageExtent.width,
                          surface_capabilities.maxImageExtent.width);
  size.height = std::clamp(size.height, surface_capabilities.minImageExtent.height,
                           surface_capabilities.maxImageExtent.height);

  // Prefer identity transform if possible
  VkSurfaceTransformFlagBitsKHR transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  if (!(surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR))
    transform = surface_capabilities.currentTransform;

  // Select swap chain flags, we only need a colour attachment
  VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  if (!(surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
  {
    ERROR_LOG_FMT(VIDEO, "Vulkan: Swap chain does not support usage as color attachment");
    return false;
  }

  // Select the number of image layers for Quad-Buffered stereoscopy
  uint32_t image_layers = g_ActiveConfig.stereo_mode == StereoMode::QuadBuffer ? 2 : 1;

  // Store the old/current swap chain when recreating for resize
  VkSwapchainKHR old_swap_chain = m_swap_chain;
  m_swap_chain = VK_NULL_HANDLE;

  // Now we can actually create the swap chain
  VkSwapchainCreateInfoKHR swap_chain_info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                              nullptr,
                                              0,
                                              m_surface,
                                              image_count,
                                              m_surface_format.format,
                                              m_surface_format.colorSpace,
                                              size,
                                              image_layers,
                                              image_usage,
                                              VK_SHARING_MODE_EXCLUSIVE,
                                              0,
                                              nullptr,
                                              transform,
                                              VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                              m_present_mode,
                                              VK_TRUE,
                                              old_swap_chain};
  std::array<uint32_t, 2> indices = {{
      g_vulkan_context->GetGraphicsQueueFamilyIndex(),
      g_vulkan_context->GetPresentQueueFamilyIndex(),
  }};
  if (g_vulkan_context->GetGraphicsQueueFamilyIndex() !=
      g_vulkan_context->GetPresentQueueFamilyIndex())
  {
    swap_chain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swap_chain_info.queueFamilyIndexCount = 2;
    swap_chain_info.pQueueFamilyIndices = indices.data();
  }

#ifdef SUPPORTS_VULKAN_EXCLUSIVE_FULLSCREEN
  if (m_fullscreen_supported)
  {
    VkSurfaceFullScreenExclusiveInfoEXT fullscreen_support = {};
    swap_chain_info.pNext = &fullscreen_support;
    fullscreen_support.sType = VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT;
    fullscreen_support.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT;

    auto platform_info = g_vulkan_context->GetPlatformExclusiveFullscreenInfo(m_wsi);
    fullscreen_support.pNext = &platform_info;

    res = vkCreateSwapchainKHR(g_vulkan_context->GetDevice(), &swap_chain_info, nullptr,
                               &m_swap_chain);
    if (res != VK_SUCCESS)
    {
      // Try without exclusive fullscreen.
      WARN_LOG_FMT(VIDEO, "Failed to create exclusive fullscreen swapchain, trying without.");
      swap_chain_info.pNext = nullptr;
      g_Config.backend_info.bSupportsExclusiveFullscreen = false;
      g_ActiveConfig.backend_info.bSupportsExclusiveFullscreen = false;
      m_fullscreen_supported = false;
    }
  }
#endif

  if (m_swap_chain == VK_NULL_HANDLE)
  {
    res = vkCreateSwapchainKHR(g_vulkan_context->GetDevice(), &swap_chain_info, nullptr,
                               &m_swap_chain);
  }
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateSwapchainKHR failed: ");
    return false;
  }

  // Now destroy the old swap chain, since it's been recreated.
  // We can do this immediately since all work should have been completed before calling resize.
  if (old_swap_chain != VK_NULL_HANDLE)
    vkDestroySwapchainKHR(g_vulkan_context->GetDevice(), old_swap_chain, nullptr);

  m_width = size.width;
  m_height = size.height;
  m_layers = image_layers;
  return true;
}

bool SwapChain::SetupSwapChainImages()
{
  ASSERT(m_swap_chain_images.empty());

  uint32_t image_count;
  VkResult res =
      vkGetSwapchainImagesKHR(g_vulkan_context->GetDevice(), m_swap_chain, &image_count, nullptr);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkGetSwapchainImagesKHR failed: ");
    return false;
  }

  std::vector<VkImage> images(image_count);
  res = vkGetSwapchainImagesKHR(g_vulkan_context->GetDevice(), m_swap_chain, &image_count,
                                images.data());
  ASSERT(res == VK_SUCCESS);

  const TextureConfig texture_config(
      TextureConfig(m_width, m_height, 1, m_layers, 1, m_texture_format,
                    AbstractTextureFlag_RenderTarget, AbstractTextureType::Texture_2DArray));
  const VkRenderPass load_render_pass = g_object_cache->GetRenderPass(
      m_surface_format.format, VK_FORMAT_UNDEFINED, 1, VK_ATTACHMENT_LOAD_OP_LOAD);
  const VkRenderPass clear_render_pass = g_object_cache->GetRenderPass(
      m_surface_format.format, VK_FORMAT_UNDEFINED, 1, VK_ATTACHMENT_LOAD_OP_CLEAR);
  if (load_render_pass == VK_NULL_HANDLE || clear_render_pass == VK_NULL_HANDLE)
  {
    PanicAlertFmt("Failed to get swap chain render passes.");
    return false;
  }

  m_swap_chain_images.reserve(image_count);
  for (uint32_t i = 0; i < image_count; i++)
  {
    SwapChainImage image;
    image.image = images[i];

    // Create texture object, which creates a view of the backbuffer
    image.texture =
        VKTexture::CreateAdopted(texture_config, image.image,
                                 m_layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
                                 VK_IMAGE_LAYOUT_UNDEFINED);
    if (!image.texture)
      return false;

    image.framebuffer = VKFramebuffer::Create(image.texture.get(), nullptr, {});
    if (!image.framebuffer)
    {
      image.texture.reset();
      return false;
    }

    m_swap_chain_images.emplace_back(std::move(image));
  }

  return true;
}

void SwapChain::DestroySwapChainImages()
{
  for (auto& it : m_swap_chain_images)
  {
    // Images themselves are cleaned up by the swap chain object
    it.framebuffer.reset();
    it.texture.reset();
  }
  m_swap_chain_images.clear();
}

void SwapChain::DestroySwapChain()
{
  if (m_swap_chain == VK_NULL_HANDLE)
    return;

  // Release exclusive fullscreen before destroying.
  if (m_current_fullscreen_state)
    SetFullscreenState(false);

  vkDestroySwapchainKHR(g_vulkan_context->GetDevice(), m_swap_chain, nullptr);
  m_swap_chain = VK_NULL_HANDLE;
}

VkResult SwapChain::AcquireNextImage()
{
  VkResult res = vkAcquireNextImageKHR(g_vulkan_context->GetDevice(), m_swap_chain, UINT64_MAX,
                                       g_command_buffer_mgr->GetCurrentCommandBufferSemaphore(),
                                       VK_NULL_HANDLE, &m_current_swap_chain_image_index);
  if (res != VK_SUCCESS && res != VK_ERROR_OUT_OF_DATE_KHR && res != VK_SUBOPTIMAL_KHR)
    LOG_VULKAN_ERROR(res, "vkAcquireNextImageKHR failed: ");

  return res;
}

bool SwapChain::ResizeSwapChain()
{
  DestroySwapChainImages();
  if (!CreateSwapChain() || !SetupSwapChainImages())
  {
    PanicAlertFmt("Failed to re-configure swap chain images, this is fatal (for now)");
    return false;
  }

  return true;
}

bool SwapChain::RecreateSwapChain()
{
  DestroySwapChainImages();
  DestroySwapChain();
  if (!CreateSwapChain() || !SetupSwapChainImages())
  {
    PanicAlertFmt("Failed to re-configure swap chain images, this is fatal (for now)");
    return false;
  }

  return true;
}

bool SwapChain::SetVSync(bool enabled)
{
  if (m_vsync_enabled == enabled)
    return true;

  // Recreate the swap chain with the new present mode.
  m_vsync_enabled = enabled;
  return RecreateSwapChain();
}

bool SwapChain::SetFullscreenState(bool state)
{
#ifdef SUPPORTS_VULKAN_EXCLUSIVE_FULLSCREEN
  if (m_current_fullscreen_state == state)
    return true;

  if (state)
  {
    VkResult res = vkAcquireFullScreenExclusiveModeEXT(g_vulkan_context->GetDevice(), m_swap_chain);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkAcquireFullScreenExclusiveModeEXT failed:");
      return false;
    }

    INFO_LOG_FMT(VIDEO, "Exclusive fullscreen acquired.");
  }
  else
  {
    VkResult res = vkReleaseFullScreenExclusiveModeEXT(g_vulkan_context->GetDevice(), m_swap_chain);
    if (res != VK_SUCCESS)
      LOG_VULKAN_ERROR(res, "vkReleaseFullScreenExclusiveModeEXT failed:");

    INFO_LOG_FMT(VIDEO, "Exclusive fullscreen released.");
  }

  m_current_fullscreen_state = state;
  return true;
#else
  return false;
#endif
}

bool SwapChain::RecreateSurface(void* native_handle)
{
  // Destroy the old swap chain, images, and surface.
  DestroySwapChainImages();
  DestroySwapChain();
  DestroySurface();

  // Re-create the surface with the new native handle
  m_wsi.render_surface = native_handle;
  m_surface = CreateVulkanSurface(g_vulkan_context->GetVulkanInstance(), m_wsi);
  if (m_surface == VK_NULL_HANDLE)
    return false;

  // The validation layers get angry at us if we don't call this before creating the swapchain.
  VkBool32 present_supported = VK_TRUE;
  VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(
      g_vulkan_context->GetPhysicalDevice(), g_vulkan_context->GetPresentQueueFamilyIndex(),
      m_surface, &present_supported);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceSurfaceSupportKHR failed: ");
    return false;
  }
  if (!present_supported)
  {
    PanicAlertFmt("Recreated surface does not support presenting.");
    return false;
  }

  // Update exclusive fullscreen support (unlikely to change).
  m_fullscreen_supported = g_vulkan_context->SupportsExclusiveFullscreen(m_wsi, m_surface);
  g_Config.backend_info.bSupportsExclusiveFullscreen = m_fullscreen_supported;
  g_ActiveConfig.backend_info.bSupportsExclusiveFullscreen = m_fullscreen_supported;
  m_current_fullscreen_state = false;
  m_next_fullscreen_state = false;

  // Finally re-create the swap chain
  if (!CreateSwapChain() || !SetupSwapChainImages())
    return false;

  return true;
}

void SwapChain::DestroySurface()
{
  vkDestroySurfaceKHR(g_vulkan_context->GetVulkanInstance(), m_surface, nullptr);
  m_surface = VK_NULL_HANDLE;
}
}  // namespace Vulkan
