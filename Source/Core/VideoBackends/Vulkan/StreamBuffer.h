// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <vector>

#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoCommon.h"

#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan {

class CommandBufferManager;
class ObjectCache;

class StreamBuffer
{
public:
	StreamBuffer(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, VkBufferUsageFlags usage, size_t max_size);
	~StreamBuffer();

	bool ReserveMemory(size_t num_bytes, size_t alignment, VkBuffer* out_buffer, u8** out_allocation_ptr, size_t* out_allocation_offset);
	void CommitMemory(size_t final_num_bytes);

	static std::unique_ptr<StreamBuffer> Create(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, VkBufferUsageFlags usage, size_t initial_size, size_t max_size);

private:
	bool ResizeBuffer(size_t size);

	ObjectCache* m_object_cache;
	CommandBufferManager* m_command_buffer_mgr;
	
	VkBufferUsageFlags m_usage;
	size_t m_current_size;
	size_t m_maximum_size;
	size_t m_current_offset;
	size_t m_current_gpu_position;
	size_t m_last_allocation_size;

	VkBuffer m_buffer;
	VkDeviceMemory m_memory;
	u8* m_host_pointer;
};

}		// namespace Vulkan
