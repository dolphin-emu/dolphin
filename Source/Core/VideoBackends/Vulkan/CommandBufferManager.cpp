// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cassert>
#include <cstdint>

#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/Helpers.h"

namespace Vulkan {

CommandBufferManager::CommandBufferManager(VkDevice device, uint32_t graphics_queue_family_index, VkQueue graphics_queue)
	: m_device(device)
	, m_graphics_queue_family_index(graphics_queue_family_index)
	, m_graphics_queue(graphics_queue)
{

}

CommandBufferManager::~CommandBufferManager()
{
	vkDeviceWaitIdle(m_device);

	DestroyCommandBuffers();
	DestroyCommandPool();
}

bool CommandBufferManager::Initialize()
{
	if (!CreateCommandPool())
		return false;

	if (!CreateCommandBuffers())
		return false;

	return true;
}

bool CommandBufferManager::CreateCommandPool()
{
	VkCommandPoolCreateInfo info = {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		nullptr,
		VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		m_graphics_queue_family_index
	};

	VkResult res = vkCreateCommandPool(m_device, &info, nullptr, &m_command_pool);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateCommandPool failed: ");
		return false;
	}

	return true;
}

void CommandBufferManager::DestroyCommandPool()
{
	if (m_command_pool)
	{
		vkDestroyCommandPool(m_device, m_command_pool, nullptr);
		m_command_pool = nullptr;
	}
}

bool CommandBufferManager::CreateCommandBuffers()
{
	VkCommandBufferAllocateInfo allocate_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		nullptr,
		m_command_pool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		static_cast<uint32_t>(m_command_buffers.size())
	};

	VkFenceCreateInfo fence_info = {
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		nullptr,
		VK_FENCE_CREATE_SIGNALED_BIT
	};

	VkResult res = vkAllocateCommandBuffers(m_device, &allocate_info, m_command_buffers.data());
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkAllocateCommandBuffers failed: ");
		return false;
	}

	for (size_t i = 0; i < NUM_COMMAND_BUFFERS; i++)
	{
		res = vkCreateFence(m_device, &fence_info, nullptr, &m_fences[i]);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateFence failed: ");
			return false;
		}

		// TODO: Sort this mess out
		VkDescriptorPoolSize pool_sizes[] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 500000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 500000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16 }
		};
		VkDescriptorPoolCreateInfo pool_create_info = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			nullptr,
			0,
			100000,			// tweak this
			ARRAYSIZE(pool_sizes),
			pool_sizes
		};
		res = vkCreateDescriptorPool(m_device, &pool_create_info, nullptr, &m_descriptor_pools[i]);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateDescriptorPool failed: ");
			return false;
		}
	}

	// Activate the first command buffer, so that setup commands, uploads, etc can occur before the frame starts.
	VkCommandBufferBeginInfo begin_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		nullptr,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		nullptr
	};
	m_current_command_buffer_index = 0;
	res = vkResetFences(m_device, 1, &m_fences[m_current_command_buffer_index]);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkResetFences failed: ");
		return false;
	}
	res = vkBeginCommandBuffer(m_command_buffers[m_current_command_buffer_index], &begin_info);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkBeginCommandBuffer failed: ");
		return false;
	}

	return true;
}

void CommandBufferManager::DestroyCommandBuffers()
{
	for (size_t i = 0; i < m_command_buffers.size(); i++)
	{
		for (const auto& it : m_pending_destructions[m_current_command_buffer_index])
			it.destroy_callback(m_device, it.object);
		m_pending_destructions[m_current_command_buffer_index].clear();

		if (m_fences[i])
		{
			vkDestroyFence(m_device, m_fences[i], nullptr);
			m_fences[i] = nullptr;
		}
		if (m_descriptor_pools[i])
		{
			vkDestroyDescriptorPool(m_device, m_descriptor_pools[i], nullptr);
			m_descriptor_pools[i] = nullptr;
		}
	}

	if (m_command_buffers[0])
	{
		vkFreeCommandBuffers(m_device, m_command_pool, static_cast<uint32_t>(m_command_buffers.size()), m_command_buffers.data());
		for (size_t i = 0; i < m_command_buffers.size(); i++)
			m_command_buffers[i] = nullptr;
	}
}

VkDescriptorSet CommandBufferManager::AllocateDescriptorSet(VkDescriptorSetLayout set_layout)
{
	VkDescriptorSetAllocateInfo allocate_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		nullptr,
		m_descriptor_pools[m_current_command_buffer_index],
		1,
		&set_layout
	};

	// TODO: Is upfront allocation better here?
	VkDescriptorSet descriptor_set;
	VkResult res = vkAllocateDescriptorSets(m_device, &allocate_info, &descriptor_set);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkAllocateDescriptorSets failed: ");
		return nullptr;
	}

	return descriptor_set;
}

void CommandBufferManager::SubmitCommandBuffer(VkSemaphore signal_semaphore)
{
	VkResult res = vkEndCommandBuffer(m_command_buffers[m_current_command_buffer_index]);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkEndCommandBuffer failed: ");
		PanicAlert("Failed to end command buffer");
	}

    uint32_t wait_bits = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkSubmitInfo submit_info = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO,
		nullptr,
		0,
		nullptr,
        &wait_bits,
		1,
		&m_command_buffers[m_current_command_buffer_index],
		0,
		nullptr
	};

	if (m_wait_semaphore)
	{
		submit_info.pWaitSemaphores = &m_wait_semaphore;
		submit_info.waitSemaphoreCount = 1;
	}

	if (signal_semaphore)
	{
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &signal_semaphore;
	}

	res = vkQueueSubmit(m_graphics_queue, 1, &submit_info, m_fences[m_current_command_buffer_index]);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkQueueSubmit failed: ");
		PanicAlert("Failed to submit command buffer.");
	}

	m_wait_semaphore = nullptr;

	// Fire fence tracking callbacks.
	for (const auto& iter : m_fence_point_callbacks)
		iter.second.first(m_fences[m_current_command_buffer_index]);
}

void CommandBufferManager::ActivateCommandBuffer(VkSemaphore wait_semaphore)
{
	m_current_command_buffer_index = (m_current_command_buffer_index + 1) % NUM_COMMAND_BUFFERS;
	VkResult res = vkWaitForFences(m_device, 1, &m_fences[m_current_command_buffer_index], true, UINT64_MAX);
	if (res != VK_SUCCESS)
		LOG_VULKAN_ERROR(res, "vkWaitForFences failed: ");

	res = vkResetFences(m_device, 1, &m_fences[m_current_command_buffer_index]);
	if (res != VK_SUCCESS)
		LOG_VULKAN_ERROR(res, "vkResetFences failed: ");

	// Fire fence tracking callbacks.
	for (const auto& iter : m_fence_point_callbacks)
		iter.second.second(m_fences[m_current_command_buffer_index]);

	// Clean up all objects pending destruction on this command buffer
	for (const auto& it : m_pending_destructions[m_current_command_buffer_index])
		it.destroy_callback(m_device, it.object);
	m_pending_destructions[m_current_command_buffer_index].clear();

	// Reset command buffer to beginning since we can re-use the memory now
	res = vkResetCommandBuffer(m_command_buffers[m_current_command_buffer_index], 0);
	if (res != VK_SUCCESS)
		LOG_VULKAN_ERROR(res, "vkResetCommandBuffer failed: ");

	// Also can do the same for the descriptor pools
	res = vkResetDescriptorPool(m_device, m_descriptor_pools[m_current_command_buffer_index], 0);
	if (res != VK_SUCCESS)
		LOG_VULKAN_ERROR(res, "vkResetDescriptorPool failed: ");

	VkCommandBufferBeginInfo begin_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		nullptr,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		nullptr
	};

	res = vkBeginCommandBuffer(m_command_buffers[m_current_command_buffer_index], &begin_info);
	if (res != VK_SUCCESS)
		LOG_VULKAN_ERROR(res, "vkBeginCommandBuffer failed: ");

	m_wait_semaphore = wait_semaphore;
}

void CommandBufferManager::ExecuteCommandBuffer(bool wait_for_completion)
{
	size_t current_command_buffer_index = m_current_command_buffer_index;
	SubmitCommandBuffer(nullptr);
	ActivateCommandBuffer(nullptr);

	if (wait_for_completion)
	{
		VkResult res = vkWaitForFences(m_device, 1, &m_fences[current_command_buffer_index], VK_TRUE, UINT64_MAX);
		if (res != VK_SUCCESS)
			LOG_VULKAN_ERROR(res, "vkWaitForFences failed: ");
	}
}

void CommandBufferManager::AddFencePointCallback(const void* key, const FencePointCallback &created_callback, const FencePointCallback& reached_callback)
{
	// Shouldn't be adding twice.
	assert(m_fence_point_callbacks.find(key) == m_fence_point_callbacks.end());
	m_fence_point_callbacks.emplace(key, std::make_pair(created_callback, reached_callback));
}

void CommandBufferManager::RemoveFencePointCallback(const void* key)
{
	auto iter = m_fence_point_callbacks.find(key);
	assert(iter != m_fence_point_callbacks.end());
	m_fence_point_callbacks.erase(iter);
}

}
