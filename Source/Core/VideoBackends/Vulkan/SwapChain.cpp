// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cassert>
#include <cstdint>

#include "Common/Logging/Log.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/Helpers.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/SwapChain.h"

namespace Vulkan
{
SwapChain::SwapChain(void* native_handle, VkSurfaceKHR surface, VkQueue present_queue)
    : m_native_handle(native_handle), m_surface(surface), m_present_queue(present_queue)
{
}

SwapChain::~SwapChain()
{
  DestroySwapChainImages();
  DestroySwapChain();
  DestroyRenderPass();
  DestroySurface();
}

bool SwapChain::Initialize()
{
  if (!SelectFormats())
    return false;

  if (!CreateRenderPass())
    return false;

  if (!CreateSwapChain(nullptr))
    return false;

  if (!SetupSwapChainImages())
    return false;

  return true;
}

bool SwapChain::SelectFormats()
{
  // Select swap chain format
  m_surface_format = SelectVulkanSurfaceFormat(g_object_cache->GetPhysicalDevice(), m_surface);
  if (m_surface_format.format == VK_FORMAT_RANGE_SIZE)
    return false;

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
      ARRAYSIZE(present_render_pass_attachments),
      present_render_pass_attachments,
      ARRAYSIZE(present_render_pass_subpass_descriptions),
      present_render_pass_subpass_descriptions,
      0,
      nullptr};

  VkResult res = vkCreateRenderPass(g_object_cache->GetDevice(), &present_render_pass_info, nullptr,
                                    &m_render_pass);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateRenderPass (present) failed: ");
    return false;
  }

  return true;
}

void SwapChain::DestroyRenderPass()
{
  if (m_render_pass)
  {
    g_command_buffer_mgr->DeferResourceDestruction(m_render_pass);
    m_render_pass = nullptr;
  }
}

bool SwapChain::CreateSwapChain(VkSwapchainKHR old_swap_chain)
{
  // Look up surface properties to determine image count and dimensions
  VkSurfaceCapabilitiesKHR surface_capabilities;
  VkResult res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_object_cache->GetPhysicalDevice(),
                                                           m_surface, &surface_capabilities);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed: ");
    return false;
  }

  // Select swap chain format and present mode
  VkPresentModeKHR present_mode =
      SelectVulkanPresentMode(g_object_cache->GetPhysicalDevice(), m_surface);
  if (present_mode == VK_PRESENT_MODE_RANGE_SIZE_KHR)
    return false;

  // Select number of images in swap chain, we prefer one buffer in the background to work on
  uint32_t image_count =
      std::min(surface_capabilities.minImageCount + 1, surface_capabilities.maxImageCount);

  // Determine the dimensions of the swap chain. Values of -1 indicate the size we specify here
  // determines window size?
  m_size = surface_capabilities.currentExtent;
  if (m_size.width == UINT32_MAX)
  {
    m_size.width = std::min(std::max(surface_capabilities.minImageExtent.width, 640u),
                            surface_capabilities.maxImageExtent.width);
    m_size.height = std::min(std::max(surface_capabilities.minImageExtent.height, 480u),
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

  // Now we can actually create the swap chain
  // TODO: Handle case where the present queue is not the graphics queue.
  // Is this the case for any drivers?
  VkSwapchainCreateInfoKHR swap_chain_info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                              nullptr,
                                              0,
                                              m_surface,
                                              image_count,
                                              m_surface_format.format,
                                              m_surface_format.colorSpace,
                                              m_size,
                                              1,
                                              image_usage,
                                              VK_SHARING_MODE_EXCLUSIVE,
                                              0,
                                              nullptr,
                                              transform,
                                              VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                              present_mode,
                                              VK_TRUE,
                                              old_swap_chain};

  res = vkCreateSwapchainKHR(g_object_cache->GetDevice(), &swap_chain_info, nullptr, &m_swap_chain);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateCommandPool failed: ");
    return false;
  }

  // now destroy the old swap chain, since it's been recreated
  if (old_swap_chain)
    g_command_buffer_mgr->DeferResourceDestruction(old_swap_chain);

  return true;
}

bool SwapChain::SetupSwapChainImages()
{
  assert(m_swap_chain_images.empty());

  uint32_t image_count;
  VkResult res =
      vkGetSwapchainImagesKHR(g_object_cache->GetDevice(), m_swap_chain, &image_count, nullptr);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkGetSwapchainImagesKHR failed: ");
    return false;
  }

  std::vector<VkImage> images(image_count);
  res = vkGetSwapchainImagesKHR(g_object_cache->GetDevice(), m_swap_chain, &image_count,
                                images.data());
  assert(res == VK_SUCCESS);

  m_swap_chain_images.reserve(image_count);
  for (uint32_t i = 0; i < image_count; i++)
  {
    SwapChainImage image;
    image.Image = images[i];

    // Create texture object, which creates a view of the backbuffer
    image.Texture = Texture2D::CreateFromExistingImage(m_size.width, m_size.height, 1, 1,
                                                       m_surface_format.format,
                                                       VK_IMAGE_VIEW_TYPE_2D, image.Image);

    VkImageView view = image.Texture->GetView();
    VkFramebufferCreateInfo framebuffer_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                                nullptr,
                                                0,
                                                m_render_pass,
                                                1,
                                                &view,
                                                m_size.width,
                                                m_size.height,
                                                1};

    res = vkCreateFramebuffer(g_object_cache->GetDevice(), &framebuffer_info, nullptr,
                              &image.Framebuffer);
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
  for (auto& it : m_swap_chain_images)
  {
    // Images themselves are cleaned up by the swap chain object
    g_command_buffer_mgr->DeferResourceDestruction(it.Framebuffer);
  }
  m_swap_chain_images.clear();
}

void SwapChain::DestroySwapChain()
{
  DestroySwapChainImages();
  vkDestroySwapchainKHR(g_object_cache->GetDevice(), m_swap_chain, nullptr);
  m_swap_chain = nullptr;
}

VkResult SwapChain::AcquireNextImage(VkSemaphore available_semaphore)
{
  VkResult res =
      vkAcquireNextImageKHR(g_object_cache->GetDevice(), m_swap_chain, UINT64_MAX,
                            available_semaphore, VK_NULL_HANDLE, &m_current_swap_chain_image_index);
  if (res != VK_SUCCESS && res != VK_ERROR_OUT_OF_DATE_KHR && res != VK_SUBOPTIMAL_KHR)
    LOG_VULKAN_ERROR(res, "vkAcquireNextImageKHR failed: ");

  return res;
}

bool SwapChain::ResizeSwapChain()
{
  if (!CreateSwapChain(m_swap_chain))
    return false;

  DestroySwapChainImages();
  if (!SetupSwapChainImages())
  {
    PanicAlert("Failed to re-configure swap chain images, this is fatal (for now)");
    return false;
  }

  return true;
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
  m_surface = CreateVulkanSurface(g_object_cache->GetVulkanInstance(), native_handle);
  if (m_surface == VK_NULL_HANDLE)
    return false;

  // Finally re-create the swap chain
  if (!CreateSwapChain(VK_NULL_HANDLE) || !SetupSwapChainImages())
    return false;

  return true;
}

void SwapChain::DestroySurface()
{
  vkDestroySurfaceKHR(g_object_cache->GetVulkanInstance(), m_surface, nullptr);
  m_surface = VK_NULL_HANDLE;
}
}
