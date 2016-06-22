// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

class CommandBufferManager
{
public:
	CommandBufferManager(VkDevice device, uint32_t graphics_queue_family_index, VkQueue graphics_queue);
	~CommandBufferManager();

	bool Initialize();

	VkDevice GetDevice() const { return m_device; }
	VkCommandPool GetCommandPool() const { return m_command_pool; }

	VkCommandBuffer GetCurrentCommandBuffer() const { return m_command_buffers[m_current_command_buffer_index]; }
	VkDescriptorPool GetCurrentDescriptorPool() const { return m_descriptor_pools[m_current_command_buffer_index]; }

	VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout set_layout);
	
	void SubmitCommandBuffer(VkSemaphore signal_semaphore);
	void ActivateCommandBuffer(VkSemaphore wait_semaphore);

	void ExecuteCommandBuffer(bool wait_for_completion);

private:
	bool CreateCommandPool();
	void DestroyCommandPool();

	bool CreateCommandBuffers();
	void DestroyCommandBuffers();

	VkDevice m_device = nullptr;
	uint32_t m_graphics_queue_family_index = 0;
	VkQueue m_graphics_queue = nullptr;

	VkCommandPool m_command_pool = nullptr;

	std::array<VkCommandBuffer, NUM_COMMAND_BUFFERS> m_command_buffers = {};
	std::array<VkFence, NUM_COMMAND_BUFFERS> m_fences = {};
	std::array<VkDescriptorPool, NUM_COMMAND_BUFFERS> m_descriptor_pools = {};
	size_t m_current_command_buffer_index;

	// wait semaphore provided with next submit
	VkSemaphore m_wait_semaphore = nullptr;
};

}  // namespace Vulkan
