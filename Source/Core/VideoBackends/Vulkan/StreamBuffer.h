// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <deque>
#include <memory>
#include <utility>

#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan {

class CommandBufferManager;
class ObjectCache;

class StreamBuffer
{
public:
	StreamBuffer(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, VkBufferUsageFlags usage, size_t max_size);
	~StreamBuffer();

	VkBuffer GetBuffer() const { return m_buffer; }
	VkDeviceMemory GetDeviceMemory() const { return m_memory; }
	u8* GetHostPointer() const { return m_host_pointer; }
	
	u8* GetCurrentHostPointer() const { return m_host_pointer + m_current_offset; }
	size_t GetCurrentOffset() const { return m_current_offset; }

	bool ReserveMemory(size_t num_bytes, size_t alignment, bool allow_reuse = true, bool reallocate_if_full = false);
	void CommitMemory(size_t final_num_bytes);

	static std::unique_ptr<StreamBuffer> Create(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, VkBufferUsageFlags usage, size_t initial_size, size_t max_size);

private:
	bool ResizeBuffer(size_t size);
	void OnFencePointCreated(VkFence fence);
	void OnFencePointReached(VkFence fence);

	// Waits for as many fences as needed to allocate num_bytes bytes from the buffer.
	bool WaitForClearSpace(size_t num_bytes);

	ObjectCache* m_object_cache = nullptr;
	CommandBufferManager* m_command_buffer_mgr = nullptr;
	
	VkBufferUsageFlags m_usage = 0;
	size_t m_current_size = 0;
	size_t m_maximum_size = 0;
	size_t m_current_offset = 0;
	size_t m_current_gpu_position = 0;
	size_t m_last_allocation_size = 0;

	VkBuffer m_buffer = VK_NULL_HANDLE;
	VkDeviceMemory m_memory = VK_NULL_HANDLE;

	// TODO: Should we use char* here instead of u8*?
	u8* m_host_pointer = nullptr;

	// List of fences and the corresponding positions in the buffer
	std::deque<std::pair<VkFence, size_t>> m_tracked_fences;
};

}		// namespace Vulkan
