// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "VideoBackends/Vulkan/Constants.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan
{
class CommandBufferManager;
class ObjectCache;

class SwapChain
{
public:
  SwapChain(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr,
            VkSurfaceKHR surface, VkQueue present_queue);
  ~SwapChain();

  bool Initialize();

  VkSurfaceKHR GetSurface() const { return m_surface; }
  VkSurfaceFormatKHR GetSurfaceFormat() const { return m_surface_format; }
  VkSwapchainKHR GetSwapChain() const { return m_swap_chain; }
  VkRenderPass GetRenderPass() const { return m_render_pass; }
  VkExtent2D GetSize() const { return m_size; }
  VkImage GetCurrentImage() const
  {
    return m_swap_chain_images[m_current_swap_chain_image_index].Image;
  }
  Texture2D* GetCurrentTexture() const
  {
    return m_swap_chain_images[m_current_swap_chain_image_index].Texture.get();
  }
  VkFramebuffer GetCurrentFramebuffer() const
  {
    return m_swap_chain_images[m_current_swap_chain_image_index].Framebuffer;
  }

  VkResult AcquireNextImage(VkSemaphore available_semaphore);
  VkResult Present(VkSemaphore rendering_complete_semaphore);

  bool ResizeSwapChain();

private:
  bool SelectFormats();

  bool CreateSwapChain(VkSwapchainKHR old_swap_chain);
  void DestroySwapChain();

  bool CreateRenderPass();
  void DestroyRenderPass();

  bool SetupSwapChainImages();
  void DestroySwapChainImages();

  struct SwapChainImage
  {
    VkImage Image;
    std::unique_ptr<Texture2D> Texture;
    VkFramebuffer Framebuffer;
  };

  ObjectCache* m_object_cache = nullptr;
  CommandBufferManager* m_command_buffer_mgr = nullptr;
  VkSurfaceKHR m_surface = nullptr;
  VkSurfaceFormatKHR m_surface_format;

  VkQueue m_present_queue = nullptr;

  VkSwapchainKHR m_swap_chain = nullptr;
  std::vector<SwapChainImage> m_swap_chain_images;
  uint32_t m_current_swap_chain_image_index = 0;

  VkRenderPass m_render_pass = nullptr;

  VkExtent2D m_size = {0, 0};
};

}  // namespace Vulkan
