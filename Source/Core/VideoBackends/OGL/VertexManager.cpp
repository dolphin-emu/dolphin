// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/OGL/VertexManager.h"

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "Common/Align.h"
#include "Common/CommonTypes.h"
#include "Common/GL/GLExtensions/GLExtensions.h"

#include "VideoBackends/OGL/OGLPipeline.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/StreamBuffer.h"

#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/OnScreenDisplay.h"

namespace OGL
{
static void CheckBufferBinding()
{
  // The index buffer is part of the VAO state, therefore we need to bind it first.
  if (!ProgramShaderCache::IsValidVertexFormatBound())
  {
    ProgramShaderCache::BindVertexFormat(
        static_cast<GLVertexFormat*>(VertexLoaderManager::GetCurrentVertexFormat()));
  }
}

VertexManager::VertexManager() = default;

VertexManager::~VertexManager()
{
  if (g_ActiveConfig.backend_info.bSupportsPaletteConversion)
  {
    glDeleteTextures(static_cast<GLsizei>(m_texel_buffer_views.size()),
                     m_texel_buffer_views.data());
  }

  // VAO must be found when destroying the index buffer.
  CheckBufferBinding();
  m_texel_buffer.reset();
  m_index_buffer.reset();
  m_vertex_buffer.reset();
}

bool VertexManager::Initialize()
{
  if (!VertexManagerBase::Initialize())
    return false;

  m_vertex_buffer = StreamBuffer::Create(GL_ARRAY_BUFFER, VERTEX_STREAM_BUFFER_SIZE);
  m_index_buffer = StreamBuffer::Create(GL_ELEMENT_ARRAY_BUFFER, INDEX_STREAM_BUFFER_SIZE);

  if (g_ActiveConfig.backend_info.bSupportsPaletteConversion)
  {
    // The minimum MAX_TEXTURE_BUFFER_SIZE that the spec mandates is 65KB, we are asking for a 1MB
    // buffer here. This buffer is also used as storage for undecoded textures when compute shader
    // texture decoding is enabled, in which case the requested size is 32MB.
    GLint max_buffer_size;
    glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &max_buffer_size);
    m_texel_buffer = StreamBuffer::Create(
        GL_TEXTURE_BUFFER, std::min(max_buffer_size, static_cast<GLint>(TEXEL_STREAM_BUFFER_SIZE)));

    // Allocate texture views backed by buffer.
    static constexpr std::array<std::pair<TexelBufferFormat, GLenum>, NUM_TEXEL_BUFFER_FORMATS>
        format_mapping = {{
            {TEXEL_BUFFER_FORMAT_R8_UINT, GL_R8UI},
            {TEXEL_BUFFER_FORMAT_R16_UINT, GL_R16UI},
            {TEXEL_BUFFER_FORMAT_RGBA8_UINT, GL_RGBA8UI},
            {TEXEL_BUFFER_FORMAT_R32G32_UINT, GL_RG32UI},
        }};
    glGenTextures(static_cast<GLsizei>(m_texel_buffer_views.size()), m_texel_buffer_views.data());
    glActiveTexture(GL_MUTABLE_TEXTURE_INDEX);
    for (const auto& it : format_mapping)
    {
      glBindTexture(GL_TEXTURE_BUFFER, m_texel_buffer_views[it.first]);
      glTexBuffer(GL_TEXTURE_BUFFER, it.second, m_texel_buffer->GetGLBufferId());
    }
  }

  return true;
}

void VertexManager::UploadUtilityUniforms(const void* uniforms, u32 uniforms_size)
{
  InvalidateConstants();
  ProgramShaderCache::UploadConstants(uniforms, uniforms_size);
}

bool VertexManager::UploadTexelBuffer(const void* data, u32 data_size, TexelBufferFormat format,
                                      u32* out_offset)
{
  if (data_size > m_texel_buffer->GetSize())
  {
    OSD::AddTypedMessage(OSD::MessageType::GPUDecodeNotice,
                         StringFromFormat("GPU decode failed for size limit: %dK",
                                          m_texel_buffer->GetSize() / 1024));
    return false;
  }

  const u32 elem_size = GetTexelBufferElementSize(format);
  const auto dst = m_texel_buffer->Map(data_size, elem_size);
  std::memcpy(dst.first, data, data_size);
  ADDSTAT(g_stats.this_frame.bytes_uniform_streamed, data_size);
  *out_offset = dst.second / elem_size;
  m_texel_buffer->Unmap(data_size);

  // Bind the correct view to the texel buffer slot.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_BUFFER, m_texel_buffer_views[static_cast<u32>(format)]);
  Renderer::GetInstance()->InvalidateTextureBinding(0);
  return true;
}

bool VertexManager::UploadTexelBuffer(const void* data, u32 data_size, TexelBufferFormat format,
                                      u32* out_offset, const void* palette_data, u32 palette_size,
                                      TexelBufferFormat palette_format, u32* out_palette_offset)
{
  const u32 elem_size = GetTexelBufferElementSize(format);
  const u32 palette_elem_size = GetTexelBufferElementSize(palette_format);
  const u32 reserve_size = data_size + palette_size + palette_elem_size;
  if (reserve_size > m_texel_buffer->GetSize())
  {
    OSD::AddTypedMessage(OSD::MessageType::GPUDecodeNotice,
                         StringFromFormat("GPU decode failed for size limit: %dK",
                                          m_texel_buffer->GetSize() / 1024));
    return false;
  }

  const auto dst = m_texel_buffer->Map(reserve_size, elem_size);
  const u32 palette_byte_offset = Common::AlignUp(data_size, palette_elem_size);
  std::memcpy(dst.first, data, data_size);
  std::memcpy(dst.first + palette_byte_offset, palette_data, palette_size);
  ADDSTAT(g_stats.this_frame.bytes_uniform_streamed, palette_byte_offset + palette_size);
  *out_offset = dst.second / elem_size;
  *out_palette_offset = (dst.second + palette_byte_offset) / palette_elem_size;
  m_texel_buffer->Unmap(palette_byte_offset + palette_size);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_BUFFER, m_texel_buffer_views[static_cast<u32>(format)]);
  Renderer::GetInstance()->InvalidateTextureBinding(0);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_BUFFER, m_texel_buffer_views[static_cast<u32>(palette_format)]);
  Renderer::GetInstance()->InvalidateTextureBinding(1);

  return true;
}

GLuint VertexManager::GetVertexBufferHandle() const
{
  return m_vertex_buffer->m_buffer;
}

GLuint VertexManager::GetIndexBufferHandle() const
{
  return m_index_buffer->m_buffer;
}

void VertexManager::ResetBuffer(u32 vertex_stride)
{
  CheckBufferBinding();

  auto buffer = m_vertex_buffer->Map(MAXVBUFFERSIZE, vertex_stride);
  m_cur_buffer_pointer = m_base_buffer_pointer = buffer.first;
  m_end_buffer_pointer = buffer.first + MAXVBUFFERSIZE;

  buffer = m_index_buffer->Map(MAXIBUFFERSIZE * sizeof(u16));
  IndexGenerator::Start((u16*)buffer.first);
}

void VertexManager::CommitBuffer(u32 num_vertices, u32 vertex_stride, u32 num_indices,
                                 u32* out_base_vertex, u32* out_base_index)
{
  u32 vertex_data_size = num_vertices * vertex_stride;
  u32 index_data_size = num_indices * sizeof(u16);

  *out_base_vertex = vertex_stride > 0 ? (m_vertex_buffer->GetCurrentOffset() / vertex_stride) : 0;
  *out_base_index = m_index_buffer->GetCurrentOffset() / sizeof(u16);

  CheckBufferBinding();
  m_vertex_buffer->Unmap(vertex_data_size);
  m_index_buffer->Unmap(index_data_size);

  ADDSTAT(g_stats.this_frame.bytes_vertex_streamed, vertex_data_size);
  ADDSTAT(g_stats.this_frame.bytes_index_streamed, index_data_size);
}

void VertexManager::UploadUniforms()
{
  ProgramShaderCache::UploadConstants();
}
}  // namespace OGL
