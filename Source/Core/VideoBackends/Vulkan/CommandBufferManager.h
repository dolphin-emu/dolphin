// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <vector>

#include "VideoCommon/VideoCommon.h"

#include "VideoBackends/Vulkan/Globals.h"
#include "VideoBackends/Vulkan/Helpers.h"
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

	// Schedule a vulkan resource for destruction later on. This will occur when the command buffer
	// is next re-used, and the GPU has finished working with the specified resource.
	template<typename T>
	void DeferResourceDestruction(T object)
	{
		DeferredResourceDestruction wrapper = DeferredResourceDestruction::Wrapper<T>(object);
		m_pending_destructions[m_current_command_buffer_index].push_back(wrapper);
	}
	
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
	std::array<std::vector<DeferredResourceDestruction>, NUM_COMMAND_BUFFERS> m_pending_destructions;
	size_t m_current_command_buffer_index;

	// wait semaphore provided with next submit
	VkSemaphore m_wait_semaphore = nullptr;
};

}  // namespace Vulkan
