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
  SwapChain(void* native_handle, VkSurfaceKHR surface, VkQueue present_queue);
  ~SwapChain();

  bool Initialize();

  void* GetNativeHandle() const { return m_native_handle; }
  VkSurfaceKHR GetSurface() const { return m_surface; }
  VkSurfaceFormatKHR GetSurfaceFormat() const { return m_surface_format; }
  VkSwapchainKHR GetSwapChain() const { return m_swap_chain; }
  VkRenderPass GetRenderPass() const { return m_render_pass; }
  u32 GetWidth() const { return m_width; }
  u32 GetHeight() const { return m_height; }
  u32 GetCurrentImageIndex() const { return m_current_swap_chain_image_index; }
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

  bool RecreateSurface(void* native_handle);
  bool ResizeSwapChain();

private:
  bool SelectFormats();

  bool CreateSwapChain(VkSwapchainKHR old_swap_chain);
  void DestroySwapChain();

  bool CreateRenderPass();
  void DestroyRenderPass();

  bool SetupSwapChainImages();
  void DestroySwapChainImages();

  void DestroySurface();

  struct SwapChainImage
  {
    VkImage Image;
    std::unique_ptr<Texture2D> Texture;
    VkFramebuffer Framebuffer;
  };

  void* m_native_handle = nullptr;
  VkSurfaceKHR m_surface = nullptr;
  VkSurfaceFormatKHR m_surface_format;

  VkQueue m_present_queue = nullptr;

  VkSwapchainKHR m_swap_chain = nullptr;
  std::vector<SwapChainImage> m_swap_chain_images;
  u32 m_current_swap_chain_image_index = 0;

  VkRenderPass m_render_pass = nullptr;

  u32 m_width = 0;
  u32 m_height = 0;
};

}  // namespace Vulkan
