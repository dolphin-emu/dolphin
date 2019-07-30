// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/VertexManager.h"

#include "Common/Align.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/VertexFormat.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/OnScreenDisplay.h"

namespace Vulkan
{
static VkBufferView CreateTexelBufferView(VkBuffer buffer, VkFormat vk_format)
{
  // Create a view of the whole buffer, we'll offset our texel load into it
  VkBufferViewCreateInfo view_info = {
      VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,  // VkStructureType            sType
      nullptr,                                    // const void*                pNext
      0,                                          // VkBufferViewCreateFlags    flags
      buffer,                                     // VkBuffer                   buffer
      vk_format,                                  // VkFormat                   format
      0,                                          // VkDeviceSize               offset
      VK_WHOLE_SIZE                               // VkDeviceSize               range
  };

  VkBufferView view;
  VkResult res = vkCreateBufferView(g_vulkan_context->GetDevice(), &view_info, nullptr, &view);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateBufferView failed: ");
    return VK_NULL_HANDLE;
  }

  return view;
}

VertexManager::VertexManager() = default;

VertexManager::~VertexManager()
{
  DestroyTexelBufferViews();
}

bool VertexManager::Initialize()
{
  m_vertex_stream_buffer =
      StreamBuffer::Create(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VERTEX_STREAM_BUFFER_SIZE);
  m_index_stream_buffer =
      StreamBuffer::Create(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, INDEX_STREAM_BUFFER_SIZE);
  m_uniform_stream_buffer =
      StreamBuffer::Create(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, UNIFORM_STREAM_BUFFER_SIZE);
  if (!m_vertex_stream_buffer || !m_index_stream_buffer || !m_uniform_stream_buffer)
  {
    PanicAlert("Failed to allocate streaming buffers");
    return false;
  }

  // The validation layer complains if max(offsets) + max(ubo_ranges) >= ubo_size.
  // To work around this we reserve the maximum buffer size at all times, but only commit
  // as many bytes as we use.
  m_uniform_buffer_reserve_size = sizeof(PixelShaderConstants);
  m_uniform_buffer_reserve_size = Common::AlignUp(m_uniform_buffer_reserve_size,
                                                  g_vulkan_context->GetUniformBufferAlignment()) +
                                  sizeof(VertexShaderConstants);
  m_uniform_buffer_reserve_size = Common::AlignUp(m_uniform_buffer_reserve_size,
                                                  g_vulkan_context->GetUniformBufferAlignment()) +
                                  sizeof(GeometryShaderConstants);

  // Prefer an 8MB buffer if possible, but use less if the device doesn't support this.
  // This buffer is potentially going to be addressed as R8s in the future, so we assume
  // that one element is one byte. This doesn't use min() because of a NDK compiler bug..
  const u32 texel_buffer_size =
      TEXEL_STREAM_BUFFER_SIZE > g_vulkan_context->GetDeviceLimits().maxTexelBufferElements ?
          g_vulkan_context->GetDeviceLimits().maxTexelBufferElements :
          TEXEL_STREAM_BUFFER_SIZE;
  m_texel_stream_buffer =
      StreamBuffer::Create(VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, texel_buffer_size);
  if (!m_texel_stream_buffer)
  {
    PanicAlert("Failed to allocate streaming texel buffer");
    return false;
  }

  static constexpr std::array<std::pair<TexelBufferFormat, VkFormat>, NUM_TEXEL_BUFFER_FORMATS>
      format_mapping = {{
          {TEXEL_BUFFER_FORMAT_R8_UINT, VK_FORMAT_R8_UINT},
          {TEXEL_BUFFER_FORMAT_R16_UINT, VK_FORMAT_R16_UINT},
          {TEXEL_BUFFER_FORMAT_RGBA8_UINT, VK_FORMAT_R8G8B8A8_UINT},
          {TEXEL_BUFFER_FORMAT_R32G32_UINT, VK_FORMAT_R32G32_UINT},
      }};
  for (const auto& it : format_mapping)
  {
    if ((m_texel_buffer_views[it.first] = CreateTexelBufferView(m_texel_stream_buffer->GetBuffer(),
                                                                it.second)) == VK_NULL_HANDLE)
    {
      PanicAlert("Failed to create texel buffer view");
      return false;
    }
  }

  // Bind the buffers to all the known spots even if it's not used, to keep the driver happy.
  UploadAllConstants();
  StateTracker::GetInstance()->SetUtilityUniformBuffer(m_uniform_stream_buffer->GetBuffer(), 0,
                                                       sizeof(VertexShaderConstants));
  for (u32 i = 0; i < NUM_COMPUTE_TEXEL_BUFFERS; i++)
  {
    StateTracker::GetInstance()->SetTexelBuffer(i,
                                                m_texel_buffer_views[TEXEL_BUFFER_FORMAT_R8_UINT]);
  }

  return true;
}

void VertexManager::DestroyTexelBufferViews()
{
  for (VkBufferView view : m_texel_buffer_views)
  {
    if (view != VK_NULL_HANDLE)
      vkDestroyBufferView(g_vulkan_context->GetDevice(), view, nullptr);
  }
}

void VertexManager::ResetBuffer(u32 vertex_stride)
{
  // Attempt to allocate from buffers
  bool has_vbuffer_allocation =
      m_vertex_stream_buffer->ReserveMemory(MAXVBUFFERSIZE, vertex_stride);
  bool has_ibuffer_allocation =
      m_index_stream_buffer->ReserveMemory(MAXIBUFFERSIZE * sizeof(u16), sizeof(u16));
  if (!has_vbuffer_allocation || !has_ibuffer_allocation)
  {
    // Flush any pending commands first, so that we can wait on the fences
    WARN_LOG(VIDEO, "Executing command list while waiting for space in vertex/index buffer");
    Renderer::GetInstance()->ExecuteCommandBuffer(false);

    // Attempt to allocate again, this may cause a fence wait
    if (!has_vbuffer_allocation)
      has_vbuffer_allocation = m_vertex_stream_buffer->ReserveMemory(MAXVBUFFERSIZE, vertex_stride);
    if (!has_ibuffer_allocation)
      has_ibuffer_allocation =
          m_index_stream_buffer->ReserveMemory(MAXIBUFFERSIZE * sizeof(u16), sizeof(u16));

    // If we still failed, that means the allocation was too large and will never succeed, so panic
    if (!has_vbuffer_allocation || !has_ibuffer_allocation)
      PanicAlert("Failed to allocate space in streaming buffers for pending draw");
  }

  // Update pointers
  m_base_buffer_pointer = m_vertex_stream_buffer->GetHostPointer();
  m_end_buffer_pointer = m_vertex_stream_buffer->GetCurrentHostPointer() + MAXVBUFFERSIZE;
  m_cur_buffer_pointer = m_vertex_stream_buffer->GetCurrentHostPointer();
  IndexGenerator::Start(reinterpret_cast<u16*>(m_index_stream_buffer->GetCurrentHostPointer()));
}

void VertexManager::CommitBuffer(u32 num_vertices, u32 vertex_stride, u32 num_indices,
                                 u32* out_base_vertex, u32* out_base_index)
{
  const u32 vertex_data_size = num_vertices * vertex_stride;
  const u32 index_data_size = num_indices * sizeof(u16);

  *out_base_vertex =
      vertex_stride > 0 ? (m_vertex_stream_buffer->GetCurrentOffset() / vertex_stride) : 0;
  *out_base_index = m_index_stream_buffer->GetCurrentOffset() / sizeof(u16);

  m_vertex_stream_buffer->CommitMemory(vertex_data_size);
  m_index_stream_buffer->CommitMemory(index_data_size);

  ADDSTAT(g_stats.this_frame.bytes_vertex_streamed, static_cast<int>(vertex_data_size));
  ADDSTAT(g_stats.this_frame.bytes_index_streamed, static_cast<int>(index_data_size));

  StateTracker::GetInstance()->SetVertexBuffer(m_vertex_stream_buffer->GetBuffer(), 0);
  StateTracker::GetInstance()->SetIndexBuffer(m_index_stream_buffer->GetBuffer(), 0,
                                              VK_INDEX_TYPE_UINT16);
}

void VertexManager::UploadUniforms()
{
  UpdateVertexShaderConstants();
  UpdateGeometryShaderConstants();
  UpdatePixelShaderConstants();
}

void VertexManager::UpdateVertexShaderConstants()
{
  if (!VertexShaderManager::dirty || !ReserveConstantStorage())
    return;

  StateTracker::GetInstance()->SetGXUniformBuffer(
      UBO_DESCRIPTOR_SET_BINDING_VS, m_uniform_stream_buffer->GetBuffer(),
      m_uniform_stream_buffer->GetCurrentOffset(), sizeof(VertexShaderConstants));
  std::memcpy(m_uniform_stream_buffer->GetCurrentHostPointer(), &VertexShaderManager::constants,
              sizeof(VertexShaderConstants));
  m_uniform_stream_buffer->CommitMemory(sizeof(VertexShaderConstants));
  ADDSTAT(g_stats.this_frame.bytes_uniform_streamed, sizeof(VertexShaderConstants));
  VertexShaderManager::dirty = false;
}

void VertexManager::UpdateGeometryShaderConstants()
{
  if (!GeometryShaderManager::dirty || !ReserveConstantStorage())
    return;

  StateTracker::GetInstance()->SetGXUniformBuffer(
      UBO_DESCRIPTOR_SET_BINDING_GS, m_uniform_stream_buffer->GetBuffer(),
      m_uniform_stream_buffer->GetCurrentOffset(), sizeof(GeometryShaderConstants));
  std::memcpy(m_uniform_stream_buffer->GetCurrentHostPointer(), &GeometryShaderManager::constants,
              sizeof(GeometryShaderConstants));
  m_uniform_stream_buffer->CommitMemory(sizeof(GeometryShaderConstants));
  ADDSTAT(g_stats.this_frame.bytes_uniform_streamed, sizeof(GeometryShaderConstants));
  GeometryShaderManager::dirty = false;
}

void VertexManager::UpdatePixelShaderConstants()
{
  if (!PixelShaderManager::dirty || !ReserveConstantStorage())
    return;

  StateTracker::GetInstance()->SetGXUniformBuffer(
      UBO_DESCRIPTOR_SET_BINDING_PS, m_uniform_stream_buffer->GetBuffer(),
      m_uniform_stream_buffer->GetCurrentOffset(), sizeof(PixelShaderConstants));
  std::memcpy(m_uniform_stream_buffer->GetCurrentHostPointer(), &PixelShaderManager::constants,
              sizeof(PixelShaderConstants));
  m_uniform_stream_buffer->CommitMemory(sizeof(PixelShaderConstants));
  ADDSTAT(g_stats.this_frame.bytes_uniform_streamed, sizeof(PixelShaderConstants));
  PixelShaderManager::dirty = false;
}

bool VertexManager::ReserveConstantStorage()
{
  if (m_uniform_stream_buffer->ReserveMemory(m_uniform_buffer_reserve_size,
                                             g_vulkan_context->GetUniformBufferAlignment()))
  {
    return true;
  }

  // The only places that call constant updates are safe to have state restored.
  WARN_LOG(VIDEO, "Executing command buffer while waiting for space in uniform buffer");
  Renderer::GetInstance()->ExecuteCommandBuffer(false);

  // Since we are on a new command buffer, all constants have been invalidated, and we need
  // to reupload them. We may as well do this now, since we're issuing a draw anyway.
  UploadAllConstants();
  return false;
}

void VertexManager::UploadAllConstants()
{
  // We are free to re-use parts of the buffer now since we're uploading all constants.
  const u32 ub_alignment = static_cast<u32>(g_vulkan_context->GetUniformBufferAlignment());
  const u32 pixel_constants_offset = 0;
  const u32 vertex_constants_offset =
      Common::AlignUp(pixel_constants_offset + sizeof(PixelShaderConstants), ub_alignment);
  const u32 geometry_constants_offset =
      Common::AlignUp(vertex_constants_offset + sizeof(VertexShaderConstants), ub_alignment);
  const u32 allocation_size = geometry_constants_offset + sizeof(GeometryShaderConstants);

  // Allocate everything at once.
  // We should only be here if the buffer was full and a command buffer was submitted anyway.
  if (!m_uniform_stream_buffer->ReserveMemory(allocation_size, ub_alignment))
  {
    PanicAlert("Failed to allocate space for constants in streaming buffer");
    return;
  }

  // Update bindings
  StateTracker::GetInstance()->SetGXUniformBuffer(
      UBO_DESCRIPTOR_SET_BINDING_PS, m_uniform_stream_buffer->GetBuffer(),
      m_uniform_stream_buffer->GetCurrentOffset() + pixel_constants_offset,
      sizeof(PixelShaderConstants));
  StateTracker::GetInstance()->SetGXUniformBuffer(
      UBO_DESCRIPTOR_SET_BINDING_VS, m_uniform_stream_buffer->GetBuffer(),
      m_uniform_stream_buffer->GetCurrentOffset() + vertex_constants_offset,
      sizeof(VertexShaderConstants));
  StateTracker::GetInstance()->SetGXUniformBuffer(
      UBO_DESCRIPTOR_SET_BINDING_GS, m_uniform_stream_buffer->GetBuffer(),
      m_uniform_stream_buffer->GetCurrentOffset() + geometry_constants_offset,
      sizeof(GeometryShaderConstants));

  // Copy the actual data in
  std::memcpy(m_uniform_stream_buffer->GetCurrentHostPointer() + pixel_constants_offset,
              &PixelShaderManager::constants, sizeof(PixelShaderConstants));
  std::memcpy(m_uniform_stream_buffer->GetCurrentHostPointer() + vertex_constants_offset,
              &VertexShaderManager::constants, sizeof(VertexShaderConstants));
  std::memcpy(m_uniform_stream_buffer->GetCurrentHostPointer() + geometry_constants_offset,
              &GeometryShaderManager::constants, sizeof(GeometryShaderConstants));

  // Finally, flush buffer memory after copying
  m_uniform_stream_buffer->CommitMemory(allocation_size);
  ADDSTAT(g_stats.this_frame.bytes_uniform_streamed, allocation_size);

  // Clear dirty flags
  VertexShaderManager::dirty = false;
  GeometryShaderManager::dirty = false;
  PixelShaderManager::dirty = false;
}

void VertexManager::UploadUtilityUniforms(const void* data, u32 data_size)
{
  InvalidateConstants();
  if (!m_uniform_stream_buffer->ReserveMemory(data_size,
                                              g_vulkan_context->GetUniformBufferAlignment()))
  {
    WARN_LOG(VIDEO, "Executing command buffer while waiting for ext space in uniform buffer");
    Renderer::GetInstance()->ExecuteCommandBuffer(false);
  }

  StateTracker::GetInstance()->SetUtilityUniformBuffer(
      m_uniform_stream_buffer->GetBuffer(), m_uniform_stream_buffer->GetCurrentOffset(), data_size);
  std::memcpy(m_uniform_stream_buffer->GetCurrentHostPointer(), data, data_size);
  m_uniform_stream_buffer->CommitMemory(data_size);
  ADDSTAT(g_stats.this_frame.bytes_uniform_streamed, data_size);
}

bool VertexManager::UploadTexelBuffer(const void* data, u32 data_size, TexelBufferFormat format,
                                      u32* out_offset)
{
  if (data_size > m_texel_stream_buffer->GetCurrentSize())
  {
    OSD::AddTypedMessage(OSD::MessageType::GPUDecodeNotice,
                         StringFromFormat("GPU decode failed for size limit: %dK",
                                          m_texel_stream_buffer->GetCurrentSize() / 1024));
    return false;
  }

  const u32 elem_size = GetTexelBufferElementSize(format);
  if (!m_texel_stream_buffer->ReserveMemory(data_size, elem_size))
  {
    // Try submitting cmdbuffer.
    WARN_LOG(VIDEO, "Submitting command buffer while waiting for space in texel buffer");
    Renderer::GetInstance()->ExecuteCommandBuffer(false, false);
    if (!m_texel_stream_buffer->ReserveMemory(data_size, elem_size))
    {
      PanicAlert("Failed to allocate %u bytes from texel buffer", data_size);
      return false;
    }
  }

  std::memcpy(m_texel_stream_buffer->GetCurrentHostPointer(), data, data_size);
  *out_offset = static_cast<u32>(m_texel_stream_buffer->GetCurrentOffset()) / elem_size;
  m_texel_stream_buffer->CommitMemory(data_size);
  ADDSTAT(g_stats.this_frame.bytes_uniform_streamed, data_size);
  StateTracker::GetInstance()->SetTexelBuffer(0, m_texel_buffer_views[format]);
  return true;
}

bool VertexManager::UploadTexelBuffer(const void* data, u32 data_size, TexelBufferFormat format,
                                      u32* out_offset, const void* palette_data, u32 palette_size,
                                      TexelBufferFormat palette_format, u32* out_palette_offset)
{
  const u32 elem_size = GetTexelBufferElementSize(format);
  const u32 palette_elem_size = GetTexelBufferElementSize(palette_format);
  const u32 reserve_size = data_size + palette_size + palette_elem_size;
  if (reserve_size > m_texel_stream_buffer->GetCurrentSize())
  {
    OSD::AddTypedMessage(OSD::MessageType::GPUDecodeNotice,
                         StringFromFormat("GPU decode failed for size limit: %dK",
                                          m_texel_stream_buffer->GetCurrentSize() / 1024));
    return false;
  }

  if (!m_texel_stream_buffer->ReserveMemory(reserve_size, elem_size))
  {
    // Try submitting cmdbuffer.
    WARN_LOG(VIDEO, "Submitting command buffer while waiting for space in texel buffer");
    Renderer::GetInstance()->ExecuteCommandBuffer(false, false);
    if (!m_texel_stream_buffer->ReserveMemory(reserve_size, elem_size))
    {
      PanicAlert("Failed to allocate %u bytes from texel buffer", reserve_size);
      return false;
    }
  }

  const u32 palette_byte_offset = Common::AlignUp(data_size, palette_elem_size);
  std::memcpy(m_texel_stream_buffer->GetCurrentHostPointer(), data, data_size);
  std::memcpy(m_texel_stream_buffer->GetCurrentHostPointer() + palette_byte_offset, palette_data,
              palette_size);
  *out_offset = static_cast<u32>(m_texel_stream_buffer->GetCurrentOffset()) / elem_size;
  *out_palette_offset =
      (static_cast<u32>(m_texel_stream_buffer->GetCurrentOffset()) + palette_byte_offset) /
      palette_elem_size;

  m_texel_stream_buffer->CommitMemory(palette_byte_offset + palette_size);
  ADDSTAT(g_stats.this_frame.bytes_uniform_streamed, palette_byte_offset + palette_size);
  StateTracker::GetInstance()->SetTexelBuffer(0, m_texel_buffer_views[format]);
  StateTracker::GetInstance()->SetTexelBuffer(1, m_texel_buffer_views[palette_format]);
  return true;
}
}  // namespace Vulkan
