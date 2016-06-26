// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VertexFormat.h"
#include "VideoBackends/Vulkan/VertexManager.h"

#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace Vulkan {

// TODO: Clean up this mess
constexpr size_t INITIAL_VERTEX_BUFFER_SIZE = VertexManager::MAXVBUFFERSIZE * 2;
constexpr size_t MAX_VERTEX_BUFFER_SIZE = VertexManager::MAXVBUFFERSIZE * 16;
constexpr size_t INITIAL_INDEX_BUFFER_SIZE = VertexManager::MAXIBUFFERSIZE * sizeof(u16) * 2;
constexpr size_t MAX_INDEX_BUFFER_SIZE = VertexManager::MAXIBUFFERSIZE * sizeof(u16) * 16;

VertexManager::VertexManager(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, StateTracker* state_tracker)
	: m_object_cache(object_cache)
	, m_command_buffer_mgr(command_buffer_mgr)
	, m_state_tracker(state_tracker)
	, m_cpu_vertex_buffer(MAXVBUFFERSIZE)
	, m_cpu_index_buffer(MAXIBUFFERSIZE)
{
	m_vertex_stream_buffer = StreamBuffer::Create(
		object_cache, command_buffer_mgr,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		INITIAL_VERTEX_BUFFER_SIZE,
		MAX_VERTEX_BUFFER_SIZE);

	m_index_stream_buffer = StreamBuffer::Create(
		object_cache, command_buffer_mgr,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		INITIAL_INDEX_BUFFER_SIZE,
		MAX_INDEX_BUFFER_SIZE);

	if (!m_vertex_stream_buffer || !m_index_stream_buffer)
		PanicAlert("Failed to allocate streaming buffers");
}

VertexManager::~VertexManager()
{

}

NativeVertexFormat* VertexManager::CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl)
{
	return new VertexFormat(vtx_decl);
}

void VertexManager::PrepareDrawBuffers(u32 stride)
{
	size_t vertex_data_size = IndexGenerator::GetNumVerts() * stride;
	size_t index_data_size = IndexGenerator::GetIndexLen() * sizeof(u16);

	m_vertex_stream_buffer->CommitMemory(vertex_data_size);
	m_index_stream_buffer->CommitMemory(index_data_size);

	ADDSTAT(stats.thisFrame.bytesVertexStreamed, static_cast<int>(vertex_data_size));
	ADDSTAT(stats.thisFrame.bytesIndexStreamed, static_cast<int>(index_data_size));

	m_state_tracker->SetVertexBuffer(m_vertex_stream_buffer->GetBuffer(), 0);
	m_state_tracker->SetIndexBuffer(m_index_stream_buffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT16);
}

void VertexManager::ResetBuffer(u32 stride)
{
	if (s_cull_all)
	{
		// Not drawing on the gpu, so store in a heap buffer instead
		s_pCurBufferPointer = s_pBaseBufferPointer = m_cpu_vertex_buffer.data();
		s_pEndBufferPointer = s_pCurBufferPointer + m_cpu_vertex_buffer.size();
		IndexGenerator::Start(m_cpu_index_buffer.data());
		return;
	}

	// Attempt to allocate from buffers
	bool has_vbuffer_allocation = m_vertex_stream_buffer->ReserveMemory(MAXVBUFFERSIZE, stride);
	bool has_ibuffer_allocation = m_index_stream_buffer->ReserveMemory(MAXIBUFFERSIZE, sizeof(u16));
	if (!has_vbuffer_allocation || !has_ibuffer_allocation)
	{
		// Flush any pending commands first, so that we can wait on the fences
		Util::ExecuteCurrentCommandsAndRestoreState(m_command_buffer_mgr, m_state_tracker);

		// Attempt to allocate again, this may cause a fence wait
		if (!has_vbuffer_allocation)
			has_vbuffer_allocation = m_vertex_stream_buffer->ReserveMemory(MAXVBUFFERSIZE, stride);
		if (!has_ibuffer_allocation)
			has_ibuffer_allocation = m_index_stream_buffer->ReserveMemory(MAXIBUFFERSIZE, sizeof(u16));

		// If we still failed, that means the allocation was too large and will never succeed, so panic
		if (!has_vbuffer_allocation || !has_ibuffer_allocation)
			PanicAlert("Failed to allocate space in streaming buffers for pending draw");
	}

	// Update pointers
	s_pBaseBufferPointer = m_vertex_stream_buffer->GetHostPointer();
	s_pEndBufferPointer = s_pBaseBufferPointer + MAXVBUFFERSIZE;
	s_pCurBufferPointer = m_vertex_stream_buffer->GetCurrentHostPointer();
	IndexGenerator::Start(reinterpret_cast<u16*>(m_index_stream_buffer->GetCurrentHostPointer()));

	// Update base indices
	m_current_draw_base_vertex = static_cast<u32>(m_vertex_stream_buffer->GetCurrentOffset() / stride);
	m_current_draw_base_index = static_cast<u32>(m_index_stream_buffer->GetCurrentOffset() / sizeof(u16));
}

void VertexManager::vFlush(bool use_dst_alpha)
{
	const VertexFormat* vertex_format = static_cast<VertexFormat*>(VertexLoaderManager::GetCurrentVertexFormat());
	u32 vertex_stride = vertex_format->GetVertexStride();

	// Commit memory to device
	PrepareDrawBuffers(vertex_stride);

	// Figure out the number of indices to draw
	u32 index_count = IndexGenerator::GetIndexLen();

	// Update assembly state
	m_state_tracker->SetVertexFormat(vertex_format);
	switch (current_primitive_type)
	{
	case PRIMITIVE_POINTS:
		m_state_tracker->SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
		m_state_tracker->DisableBackFaceCulling();
		break;

	case PRIMITIVE_LINES:
		m_state_tracker->SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
		m_state_tracker->DisableBackFaceCulling();
		break;

	case PRIMITIVE_TRIANGLES:
		m_state_tracker->SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
		g_renderer->SetGenerationMode();
		break;
	}

	// Check for any shader stage changes
	m_state_tracker->CheckForShaderChanges(current_primitive_type, use_dst_alpha ? DSTALPHA_DUAL_SOURCE_BLEND : DSTALPHA_NONE);

	// Update any changed constants
	m_state_tracker->UpdateVertexShaderConstants();
	m_state_tracker->UpdateGeometryShaderConstants();
	m_state_tracker->UpdatePixelShaderConstants();

	// Bind all pending state to the command buffer
	if (!m_state_tracker->Bind(m_command_buffer_mgr->GetCurrentCommandBuffer()))
	{
		WARN_LOG(VIDEO, "Skipped draw of %u indices", index_count);
		return;
	}

	// Execute the draw
	// TODO: Handle two-pass dst alpha for devices that don't support dual-source blend
	vkCmdDrawIndexed(m_command_buffer_mgr->GetCurrentCommandBuffer(), index_count, 1, m_current_draw_base_index, m_current_draw_base_vertex, 0);
}


}  // namespace
