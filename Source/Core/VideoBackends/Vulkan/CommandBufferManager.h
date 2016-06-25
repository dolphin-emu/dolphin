// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <functional>
#include <map>
#include <utility>
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

	void SubmitCommandBuffer(VkSemaphore signal_semaphore);
	void ActivateCommandBuffer(VkSemaphore wait_semaphore);

	void ExecuteCommandBuffer(bool wait_for_completion);

	// Schedule a vulkan resource for destruction later on. This will occur when the command buffer
	// is next re-used, and the GPU has finished working with the specified resource.
	template<typename T>
	void DeferResourceDestruction(T object)
	{
		DeferredResourceDestruction wrapper = DeferredResourceDestruction::Wrapper<T>(object);
		m_pending_destructions[m_current_command_buffer_index].push_back(wrapper);
	}

	// Instruct the manager to fire the specified callback when a fence point is created.
	// Fence points are created when command buffers are executed, and can be tested if signaled,
	// which means that all commands up to the point when the callback was fired have completed.
	using FencePointCallback = std::function<void(VkFence)>;
	void AddFencePointCallback(const void* key, const FencePointCallback &created_callback, const FencePointCallback& reached_callback);
	void RemoveFencePointCallback(const void* key);

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

	// callbacks when a fence point is set
	std::map<const void*, std::pair<FencePointCallback, FencePointCallback>> m_fence_point_callbacks;
};

}  // namespace Vulkan
