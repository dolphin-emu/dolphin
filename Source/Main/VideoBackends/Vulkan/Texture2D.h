// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/Constants.h"

namespace Vulkan
{
class CommandBufferManager;
class ObjectCache;

class Texture2D
{
public:
  // Custom image layouts, mainly used for switching to/from compute
  enum class ComputeImageLayout
  {
    Undefined,
    ReadOnly,
    WriteOnly,
    ReadWrite
  };

  Texture2D(u32 width, u32 height, u32 levels, u32 layers, VkFormat format,
            VkSampleCountFlagBits samples, VkImageViewType view_type, VkImage image,
            VkDeviceMemory device_memory, VkImageView view);
  ~Texture2D();

  static std::unique_ptr<Texture2D> Create(u32 width, u32 height, u32 levels, u32 layers,
                                           VkFormat format, VkSampleCountFlagBits samples,
                                           VkImageViewType view_type, VkImageTiling tiling,
                                           VkImageUsageFlags usage);

  static std::unique_ptr<Texture2D> CreateFromExistingImage(u32 width, u32 height, u32 levels,
                                                            u32 layers, VkFormat format,
                                                            VkSampleCountFlagBits samples,
                                                            VkImageViewType view_type,
                                                            VkImage existing_image);

  u32 GetWidth() const { return m_width; }
  u32 GetHeight() const { return m_height; }
  u32 GetLevels() const { return m_levels; }
  u32 GetLayers() const { return m_layers; }
  VkFormat GetFormat() const { return m_format; }
  VkSampleCountFlagBits GetSamples() const { return m_samples; }
  VkImageLayout GetLayout() const { return m_layout; }
  VkImageViewType GetViewType() const { return m_view_type; }
  VkImage GetImage() const { return m_image; }
  VkDeviceMemory GetDeviceMemory() const { return m_device_memory; }
  VkImageView GetView() const { return m_view; }
  // Used when the render pass is changing the image layout, or to force it to
  // VK_IMAGE_LAYOUT_UNDEFINED, if the existing contents of the image is
  // irrelevant and will not be loaded.
  void OverrideImageLayout(VkImageLayout new_layout);

  void TransitionToLayout(VkCommandBuffer command_buffer, VkImageLayout new_layout);
  void TransitionToLayout(VkCommandBuffer command_buffer, ComputeImageLayout new_layout);

private:
  u32 m_width;
  u32 m_height;
  u32 m_levels;
  u32 m_layers;
  VkFormat m_format;
  VkSampleCountFlagBits m_samples;
  VkImageViewType m_view_type;
  VkImageLayout m_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  ComputeImageLayout m_compute_layout = ComputeImageLayout::Undefined;

  VkImage m_image;
  VkDeviceMemory m_device_memory;
  VkImageView m_view;
};
}
