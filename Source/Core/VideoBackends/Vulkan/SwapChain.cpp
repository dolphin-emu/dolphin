// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/SwapChain.h"

#include <algorithm>
#include <cstdint>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

#if defined(VK_USE_PLATFORM_XLIB_KHR)
#include <X11/Xlib.h>
#elif defined(VK_USE_PLATFORM_XCB_KHR)
#include <X11/Xlib-xcb.h>
#include <X11/Xlib.h>
#endif

namespace Vulkan
{
SwapChain::SwapChain(void* native_handle, VkSurfaceKHR surface, bool vsync)
    : m_native_handle(native_handle), m_surface(surface), m_vsync_enabled(vsync)
{
}

SwapChain::~SwapChain()
{
  DestroySwapChainImages();
  DestroySwapChain();
  DestroyRenderPass();
  DestroySurface();
}

VkSurfaceKHR SwapChain::CreateVulkanSurface(VkInstance instance, void* hwnd)
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
  VkWin32SurfaceCreateInfoKHR surface_create_info = {
      VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,  // VkStructureType               sType
      nullptr,                                          // const void*                   pNext
      0,                                                // VkWin32SurfaceCreateFlagsKHR  flags
      nullptr,                                          // HINSTANCE                     hinstance
      reinterpret_cast<HWND>(hwnd)                      // HWND                          hwnd
  };

  VkSurfaceKHR surface;
  VkResult res = vkCreateWin32SurfaceKHR(instance, &surface_create_info, nullptr, &surface);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateWin32SurfaceKHR failed: ");
    return VK_NULL_HANDLE;
  }

  return surface;

#elif defined(VK_USE_PLATFORM_XLIB_KHR)
  // Assuming the display handles are compatible, or shared. This matches what we do in the
  // GL backend, but it's not ideal.
  Display* display = XOpenDisplay(nullptr);

  VkXlibSurfaceCreateInfoKHR surface_create_info = {
      VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,  // VkStructureType               sType
      nullptr,                                         // const void*                   pNext
      0,                                               // VkXlibSurfaceCreateFlagsKHR   flags
      display,                                         // Display*                      dpy
      reinterpret_cast<Window>(hwnd)                   // Window                        window
  };

  VkSurfaceKHR surface;
  VkResult res = vkCreateXlibSurfaceKHR(instance, &surface_create_info, nullptr, &surface);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateXlibSurfaceKHR failed: ");
    return VK_NULL_HANDLE;
  }

  return surface;

#elif defined(VK_USE_PLATFORM_XCB_KHR)
  // If we ever switch to using xcb, we should pass the display handle as well.
  Display* display = XOpenDisplay(nullptr);
  xcb_connection_t* connection = XGetXCBConnection(display);

  VkXcbSurfaceCreateInfoKHR surface_create_info = {
      VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,  // VkStructureType               sType
      nullptr,                                        // const void*                   pNext
      0,                                              // VkXcbSurfaceCreateFlagsKHR    flags
      connection,                                     // xcb_connection_t*             connection
      static_cast<xcb_window_t>(reinterpret_cast<uintptr_t>(hwnd))  // xcb_window_t window
  };

  VkSurfaceKHR surface;
  VkResult res = vkCreateXcbSurfaceKHR(instance, &surface_create_info, nullptr, &surface);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateXcbSurfaceKHR failed: ");
    return VK_NULL_HANDLE;
  }

  return surface;

#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
  VkAndroidSurfaceCreateInfoKHR surface_create_info = {
      VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,  // VkStructureType                sType
      nullptr,                                            // const void*                    pNext
      0,                                                  // VkAndroidSurfaceCreateFlagsKHR flags
      reinterpret_cast<ANativeWindow*>(hwnd)              // ANativeWindow*                 window
  };

  VkSurfaceKHR surface;
  VkResult res = vkCreateAndroidSurfaceKHR(instance, &surface_create_info, nullptr, &surface);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateAndroidSurfaceKHR failed: ");
    return VK_NULL_HANDLE;
  }

  return surface;

#else
  return VK_NULL_HANDLE;
#endif
}

std::unique_ptr<SwapChain> SwapChain::Create(void* native_handle, VkSurfaceKHR surface, bool vsync)
{
  std::unique_ptr<SwapChain> swap_chain =
      std::make_unique<SwapChain>(native_handle, surface, vsync);

  if (!swap_chain->CreateSwapChain() || !swap_chain->CreateRenderPass() ||
      !swap_chain->SetupSwapChainImages())
  {
    return nullptr;
  }

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
  _assert_(res == VK_SUCCESS);

  // If there is a single undefined surface format, the device doesn't care, so we'll just use RGBA
  if (surface_formats[0].format == VK_FORMAT_UNDEFINED)
  {
    m_surface_format.format = VK_FORMAT_R8G8B8A8_UNORM;
    m_surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    return true;
  }

  // Use the first surface format, just use what it prefers.
  // Some drivers seem to return a SRGB format here (Intel Mesa).
  // This results in gamma correction when presenting to the screen, which we don't want.
  // Use a linear format instead, if this is the case.
  m_surface_format.format = Util::GetLinearFormat(surface_formats[0].format);
  m_surface_format.colorSpace = surface_formats[0].colorSpace;
  return true;
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
  _assert_(res == VK_SUCCESS);

  // Checks if a particular mode is supported, if it is, returns that mode.
  auto CheckForMode = [&present_modes](VkPresentModeKHR check_mode) {
    auto it = std::find_if(present_modes.begin(), present_modes.end(),
                           [check_mode](VkPresentModeKHR mode) { return check_mode == mode; });
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

bool SwapChain::CreateRenderPass()
{
  // render pass for rendering to the swap chain
  VkAttachmentDescription present_render_pass_attachments[] = {
      {0, m_surface_format.format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR,
       VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
       VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

  VkAttachmentReference present_render_pass_color_attachment_references[] = {
      {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

  VkSubpassDescription present_render_pass_subpass_descriptions[] = {
      {0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, 1,
       present_render_pass_color_attachment_references, nullptr, nullptr, 0, nullptr}};

  VkRenderPassCreateInfo present_render_pass_info = {
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      nullptr,
      0,
      static_cast<u32>(ArraySize(present_render_pass_attachments)),
      present_render_pass_attachments,
      static_cast<u32>(ArraySize(present_render_pass_subpass_descriptions)),
      present_render_pass_subpass_descriptions,
      0,
      nullptr};

  VkResult res = vkCreateRenderPass(g_vulkan_context->GetDevice(), &present_render_pass_info,
                                    nullptr, &m_render_pass);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateRenderPass (present) failed: ");
    return false;
  }

  return true;
}

void SwapChain::DestroyRenderPass()
{
  if (!m_render_pass)
    return;

  vkDestroyRenderPass(g_vulkan_context->GetDevice(), m_render_pass, nullptr);
  m_render_pass = VK_NULL_HANDLE;
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
    size.width = std::min(std::max(surface_capabilities.minImageExtent.width, 640u),
                          surface_capabilities.maxImageExtent.width);
    size.height = std::min(std::max(surface_capabilities.minImageExtent.height, 480u),
                           surface_capabilities.maxImageExtent.height);
  }

  // Prefer identity transform if possible
  VkSurfaceTransformFlagBitsKHR transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  if (!(surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR))
    transform = surface_capabilities.currentTransform;

  // Select swap chain flags, we only need a colour attachment
  VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  if (!(surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
  {
    ERROR_LOG(VIDEO, "Vulkan: Swap chain does not support usage as color attachment");
    return false;
  }

  // Select the number of image layers for Quad-Buffered stereoscopy
  uint32_t image_layers = g_ActiveConfig.iStereoMode == STEREO_QUADBUFFER ? 2 : 1;

  // Store the old/current swap chain when recreating for resize
  VkSwapchainKHR old_swap_chain = m_swap_chain;

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

  res =
      vkCreateSwapchainKHR(g_vulkan_context->GetDevice(), &swap_chain_info, nullptr, &m_swap_chain);
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
  _assert_(m_swap_chain_images.empty());

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
  _assert_(res == VK_SUCCESS);

  m_swap_chain_images.reserve(image_count);
  for (uint32_t i = 0; i < image_count; i++)
  {
    SwapChainImage image;
    image.image = images[i];

    // Create texture object, which creates a view of the backbuffer
    image.texture = Texture2D::CreateFromExistingImage(
        m_width, m_height, 1, 1, m_surface_format.format, VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_VIEW_TYPE_2D, image.image);

    VkImageView view = image.texture->GetView();
    VkFramebufferCreateInfo framebuffer_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                                nullptr,
                                                0,
                                                m_render_pass,
                                                1,
                                                &view,
                                                m_width,
                                                m_height,
                                                m_layers};

    res = vkCreateFramebuffer(g_vulkan_context->GetDevice(), &framebuffer_info, nullptr,
                              &image.framebuffer);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkCreateFramebuffer failed: ");
      return false;
    }

    m_swap_chain_images.emplace_back(std::move(image));
  }

  return true;
}

void SwapChain::DestroySwapChainImages()
{
  for (const auto& it : m_swap_chain_images)
  {
    // Images themselves are cleaned up by the swap chain object
    vkDestroyFramebuffer(g_vulkan_context->GetDevice(), it.framebuffer, nullptr);
  }
  m_swap_chain_images.clear();
}

void SwapChain::DestroySwapChain()
{
  if (m_swap_chain == VK_NULL_HANDLE)
    return;

  vkDestroySwapchainKHR(g_vulkan_context->GetDevice(), m_swap_chain, nullptr);
  m_swap_chain = VK_NULL_HANDLE;
}

VkResult SwapChain::AcquireNextImage(VkSemaphore available_semaphore)
{
  VkResult res =
      vkAcquireNextImageKHR(g_vulkan_context->GetDevice(), m_swap_chain, UINT64_MAX,
                            available_semaphore, VK_NULL_HANDLE, &m_current_swap_chain_image_index);
  if (res != VK_SUCCESS && res != VK_ERROR_OUT_OF_DATE_KHR && res != VK_SUBOPTIMAL_KHR)
    LOG_VULKAN_ERROR(res, "vkAcquireNextImageKHR failed: ");

  return res;
}

bool SwapChain::ResizeSwapChain()
{
  DestroySwapChainImages();
  if (!CreateSwapChain() || !SetupSwapChainImages())
  {
    PanicAlert("Failed to re-configure swap chain images, this is fatal (for now)");
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
    PanicAlert("Failed to re-configure swap chain images, this is fatal (for now)");
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

bool SwapChain::RecreateSurface(void* native_handle)
{
  // Destroy the old swap chain, images, and surface.
  DestroyRenderPass();
  DestroySwapChainImages();
  DestroySwapChain();
  DestroySurface();

  // Re-create the surface with the new native handle
  m_native_handle = native_handle;
  m_surface = CreateVulkanSurface(g_vulkan_context->GetVulkanInstance(), native_handle);
  if (m_surface == VK_NULL_HANDLE)
    return false;

  // Finally re-create the swap chain
  if (!CreateSwapChain() || !SetupSwapChainImages() || !CreateRenderPass())
    return false;

  return true;
}

void SwapChain::DestroySurface()
{
  vkDestroySurfaceKHR(g_vulkan_context->GetVulkanInstance(), m_surface, nullptr);
  m_surface = VK_NULL_HANDLE;
}
}
