#pragma once

#include <array>
#include <map>
#include <memory>
#include <vector>

#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoCommon.h"

#include "VideoBackends/Vulkan/Globals.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan {

class CommandBufferManager;
class ObjectCache;

class Texture2D
{
public:
	Texture2D(CommandBufferManager* command_buffer_mgr, u32 width, u32 height, u32 levels, u32 layers, VkFormat format, VkImage image, VkDeviceMemory device_memory, VkImageView view);
	~Texture2D();

	static std::unique_ptr<Texture2D> Create(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, u32 width, u32 height, u32 levels, u32 layers, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage);

	u32 GetWidth() const { return m_width; }
	u32 GetHeight() const { return m_height; }
	u32 GetLevels() const { return m_levels; }
	u32 GetLayers() const { return m_layers; }
	VkFormat GetFormat() const { return m_format; }

	VkImage GetImage() const { return m_image; }
	VkDeviceMemory GetDeviceMemory() const { return m_device_memory; }
	VkImageView GetView() const { return m_view; }

	void TransitionToLayout(VkCommandBuffer command_buffer, VkImageLayout new_layout);

private:
	CommandBufferManager* m_command_buffer_mgr;

	u32 m_width = 0;
	u32 m_height = 0;
	u32 m_levels = 0;
	u32 m_layers = 0;
	VkFormat m_format = VK_FORMAT_UNDEFINED;
	VkImageLayout m_layout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkImage m_image = nullptr;
	VkDeviceMemory m_device_memory = nullptr;
	VkImageView m_view = nullptr;
};

}