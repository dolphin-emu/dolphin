// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Metal/VertexManager.h"
#include "VideoBackends/Metal/MetalContext.h"
#include "VideoBackends/Metal/MetalFramebuffer.h"
#include "VideoBackends/Metal/MetalVertexFormat.h"
#include "VideoBackends/Metal/Render.h"
#include "VideoBackends/Metal/StateTracker.h"
#include "VideoBackends/Metal/StreamBuffer.h"

#include "Common/Align.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"

namespace Metal
{
// TODO: Clean up this mess
constexpr size_t INITIAL_VERTEX_BUFFER_SIZE = VertexManager::MAXVBUFFERSIZE * 2;
constexpr size_t MAX_VERTEX_BUFFER_SIZE = VertexManager::MAXVBUFFERSIZE * 16;
constexpr size_t INITIAL_INDEX_BUFFER_SIZE = VertexManager::MAXIBUFFERSIZE * sizeof(u16) * 2;
constexpr size_t MAX_INDEX_BUFFER_SIZE = VertexManager::MAXIBUFFERSIZE * sizeof(u16) * 16;

VertexManager::VertexManager()
    : m_cpu_vertex_buffer(MAXVBUFFERSIZE), m_cpu_index_buffer(MAXIBUFFERSIZE)
{
  InvalidateUniforms();
}

VertexManager::~VertexManager()
{
}

VertexManager* VertexManager::GetInstance()
{
  return static_cast<VertexManager*>(g_vertex_manager.get());
}

bool VertexManager::CreateStreamingBuffers()
{
  m_vertex_stream_buffer = StreamBuffer::Create(INITIAL_VERTEX_BUFFER_SIZE, MAX_VERTEX_BUFFER_SIZE);
  m_index_stream_buffer = StreamBuffer::Create(INITIAL_INDEX_BUFFER_SIZE, MAX_INDEX_BUFFER_SIZE);
  if (!m_vertex_stream_buffer || !m_index_stream_buffer)
  {
    PanicAlert("Failed to allocate vertex streaming buffers.");
    return false;
  }

  return true;
}

void VertexManager::InvalidateUniforms()
{
  VertexShaderManager::dirty = true;
  PixelShaderManager::dirty = true;
}

std::unique_ptr<NativeVertexFormat>
VertexManager::CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl)
{
  return std::make_unique<MetalVertexFormat>(vtx_decl);
}

void VertexManager::PrepareDrawBuffers(u32 stride)
{
  size_t vertex_data_size = IndexGenerator::GetNumVerts() * stride;
  size_t index_data_size = IndexGenerator::GetIndexLen() * sizeof(u16);

  m_vertex_stream_buffer->CommitMemory(vertex_data_size);
  m_index_stream_buffer->CommitMemory(index_data_size);

  ADDSTAT(stats.thisFrame.bytesVertexStreamed, static_cast<int>(vertex_data_size));
  ADDSTAT(stats.thisFrame.bytesIndexStreamed, static_cast<int>(index_data_size));

  g_state_tracker->SetVertexBuffer(m_vertex_stream_buffer->GetBuffer(), 0);
  g_state_tracker->SetIndexBuffer(m_index_stream_buffer->GetBuffer(), m_current_draw_index_offset,
                                  mtlpp::IndexType::UInt16);
}

void VertexManager::ResetBuffer(u32 stride)
{
  if (m_cull_all)
  {
    // Not drawing on the gpu, so store in a heap buffer instead
    m_cur_buffer_pointer = m_base_buffer_pointer = m_cpu_vertex_buffer.data();
    m_end_buffer_pointer = m_base_buffer_pointer + m_cpu_vertex_buffer.size();
    IndexGenerator::Start(m_cpu_index_buffer.data());
    return;
  }

  // Attempt to allocate from buffers
  bool has_vbuffer_allocation = m_vertex_stream_buffer->ReserveMemory(MAXVBUFFERSIZE, stride);
  bool has_ibuffer_allocation = m_index_stream_buffer->ReserveMemory(MAXIBUFFERSIZE, sizeof(u16));
  if (!has_vbuffer_allocation || !has_ibuffer_allocation)
  {
    // Flush any pending commands first, so that we can wait on the fences
    WARN_LOG(VIDEO, "Executing command list while waiting for space in vertex/index buffer");
    Renderer::GetInstance()->SubmitCommandBuffer();

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
  m_base_buffer_pointer = m_vertex_stream_buffer->GetHostPointer();
  m_end_buffer_pointer = m_vertex_stream_buffer->GetCurrentHostPointer() + MAXVBUFFERSIZE;
  m_cur_buffer_pointer = m_vertex_stream_buffer->GetCurrentHostPointer();
  IndexGenerator::Start(reinterpret_cast<u16*>(m_index_stream_buffer->GetCurrentHostPointer()));

  // Update base indices
  m_current_draw_base_vertex =
      static_cast<u32>(m_vertex_stream_buffer->GetCurrentOffset() / stride);
  m_current_draw_index_offset = m_index_stream_buffer->GetCurrentOffset();
}

void VertexManager::UploadUniforms()
{
  // If both types are changed, upload in one allocation.
  if (VertexShaderManager::dirty && PixelShaderManager::dirty)
  {
    UploadAllUniforms();
    return;
  }

  StreamBuffer* const ubo = g_metal_context->GetUniformStreamBuffer();
  if (VertexShaderManager::dirty)
  {
    if (!ubo->ReserveMemory(sizeof(VertexShaderConstants), UNIFORM_BUFFER_ALIGNMENT))
    {
      // Upload all constants, potentially with a new buffer.
      UploadAllUniforms();
      return;
    }

    g_state_tracker->SetVertexUniforms(ubo->GetBuffer(), ubo->GetCurrentOffset());
    if (g_ActiveConfig.bEnablePixelLighting)
      g_state_tracker->SetPixelUniforms(1, ubo->GetBuffer(), ubo->GetCurrentOffset());

    std::memcpy(ubo->GetCurrentHostPointer(), &VertexShaderManager::constants,
                sizeof(VertexShaderConstants));
    ubo->CommitMemory(sizeof(VertexShaderConstants));
    ADDSTAT(stats.thisFrame.bytesUniformStreamed, static_cast<int>(sizeof(VertexShaderConstants)));
    VertexShaderManager::dirty = false;
  }

  if (PixelShaderManager::dirty)
  {
    if (!ubo->ReserveMemory(sizeof(PixelShaderConstants), UNIFORM_BUFFER_ALIGNMENT))
    {
      UploadAllUniforms();
      return;
    }

    g_state_tracker->SetPixelUniforms(0, ubo->GetBuffer(), ubo->GetCurrentOffset());
    std::memcpy(ubo->GetCurrentHostPointer(), &PixelShaderManager::constants,
                sizeof(PixelShaderConstants));
    ubo->CommitMemory(sizeof(PixelShaderConstants));
    ADDSTAT(stats.thisFrame.bytesUniformStreamed, static_cast<int>(sizeof(PixelShaderConstants)));
    PixelShaderManager::dirty = false;
  }
}

void VertexManager::UploadAllUniforms()
{
  // Perform one allocation for both the VS and PS buffers.
  constexpr u32 UNIFORM_BUFFER_RESERVE_SIZE =
      Common::AlignUp(sizeof(VertexShaderConstants), UNIFORM_BUFFER_ALIGNMENT) +
      Common::AlignUp(sizeof(PixelShaderConstants), UNIFORM_BUFFER_ALIGNMENT);
  constexpr u32 PBUF_OFFSET =
      Common::AlignUp(sizeof(VertexShaderConstants), UNIFORM_BUFFER_ALIGNMENT);

  StreamBuffer* const ubo = g_metal_context->GetUniformStreamBuffer();
  if (!ubo->ReserveMemory(UNIFORM_BUFFER_RESERVE_SIZE, UNIFORM_BUFFER_ALIGNMENT))
  {
    // Kick the command buffer.
    DEBUG_LOG(VIDEO, "Kicking command buffer due to lack of space in uniform buffer.");
    Renderer::GetInstance()->SubmitCommandBuffer();
    if (!ubo->ReserveMemory(UNIFORM_BUFFER_RESERVE_SIZE, UNIFORM_BUFFER_ALIGNMENT))
    {
      PanicAlert("Failed to allocate storage for uniforms.");
      return;
    }
  }

  g_state_tracker->SetVertexUniforms(ubo->GetBuffer(), ubo->GetCurrentOffset());
  g_state_tracker->SetPixelUniforms(0, ubo->GetBuffer(), ubo->GetCurrentOffset() + PBUF_OFFSET);
  if (g_ActiveConfig.bEnablePixelLighting)
    g_state_tracker->SetPixelUniforms(1, ubo->GetBuffer(), ubo->GetCurrentOffset() + PBUF_OFFSET);

  std::memcpy(ubo->GetCurrentHostPointer(), &VertexShaderManager::constants,
              sizeof(VertexShaderConstants));
  std::memcpy(ubo->GetCurrentHostPointer() + PBUF_OFFSET, &PixelShaderManager::constants,
              sizeof(PixelShaderConstants));
  ubo->CommitMemory(UNIFORM_BUFFER_RESERVE_SIZE);
  ADDSTAT(stats.thisFrame.bytesUniformStreamed, static_cast<int>(UNIFORM_BUFFER_RESERVE_SIZE));
  VertexShaderManager::dirty = false;
  PixelShaderManager::dirty = false;
}

void VertexManager::vFlush()
{
  // Figure out the number of indices to draw.
  const NativeVertexFormat* vertex_format = VertexLoaderManager::GetCurrentVertexFormat();
  const u32 vertex_stride = vertex_format->GetVertexStride();
  const u32 index_count = IndexGenerator::GetIndexLen();

  // Upload uniform buffers.
  UploadUniforms();

  // Commit memory to device.
  // NOTE: This must be done after constant upload, as a constant buffer overrun can cause
  // the current command buffer to be executed, and we want the buffer space to be associated
  // with the command buffer that has the corresponding draw.
  PrepareDrawBuffers(vertex_stride);

  // Bind all state and issue the draw call.
  if (m_current_pipeline_object)
  {
    g_state_tracker->SetPipeline(static_cast<const MetalPipeline*>(m_current_pipeline_object));
    g_state_tracker->DrawIndexed(index_count, m_current_draw_base_vertex);
  }
  else
  {
    WARN_LOG(VIDEO, "Skipped draw of %u indices due to no pipeline.", index_count);
  }
}

}  // namespace Metal
