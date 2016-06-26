// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan {

class CommandBufferManager;
class ObjectCache;

class Texture2D
{
public:
	Texture2D(CommandBufferManager* command_buffer_mgr, u32 width, u32 height, u32 levels, u32 layers, VkFormat format, VkImageViewType view_type, VkImage image, VkDeviceMemory device_memory, VkImageView view, VkImageView depth_view);
	~Texture2D();

	static std::unique_ptr<Texture2D> Create(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, u32 width, u32 height, u32 levels, u32 layers, VkFormat format, VkImageViewType view_type, VkImageTiling tiling, VkImageUsageFlags usage);

	static std::unique_ptr<Texture2D> CreateFromExistingImage(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, u32 width, u32 height, u32 levels, u32 layers, VkFormat format, VkImageViewType view_type, VkImage existing_image);

	u32 GetWidth() const { return m_width; }
	u32 GetHeight() const { return m_height; }
	u32 GetLevels() const { return m_levels; }
	u32 GetLayers() const { return m_layers; }
	VkFormat GetFormat() const { return m_format; }
	VkImageViewType GetViewType() const { return m_view_type; }

	VkImage GetImage() const { return m_image; }
	VkDeviceMemory GetDeviceMemory() const { return m_device_memory; }
	VkImageView GetView() const { return m_view; }
	VkImageView GetDepthView() const { return m_depth_view; }

	// Used when the render pass is changing the image layout, or to force it to VK_IMAGE_LAYOUT_UNDEFINED
	// if the existing contents of the image is irrelevant and will not be loaded.
	void OverrideImageLayout(VkImageLayout new_layout);

	void TransitionToLayout(VkCommandBuffer command_buffer, VkImageLayout new_layout);

private:
	CommandBufferManager* m_command_buffer_mgr;

	u32 m_width = 0;
	u32 m_height = 0;
	u32 m_levels = 0;
	u32 m_layers = 0;
	VkFormat m_format = VK_FORMAT_UNDEFINED;
	VkImageLayout m_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageViewType m_view_type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

	VkImage m_image = nullptr;
	VkDeviceMemory m_device_memory = nullptr;
	VkImageView m_view = nullptr;
	VkImageView m_depth_view = nullptr;
};

}