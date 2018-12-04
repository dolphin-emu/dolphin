// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/OGL/VertexManager.h"

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/GL/GLExtensions/GLExtensions.h"
#include "Common/StringUtil.h"

#include "VideoBackends/OGL/BoundingBox.h"
#include "VideoBackends/OGL/OGLPipeline.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/StreamBuffer.h"
#include "VideoCommon/BoundingBox.h"

#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace OGL
{
// This are the initially requested size for the buffers expressed in bytes
const u32 MAX_IBUFFER_SIZE = 2 * 1024 * 1024;
const u32 MAX_VBUFFER_SIZE = 32 * 1024 * 1024;

VertexManager::VertexManager() : m_cpu_v_buffer(MAX_VBUFFER_SIZE), m_cpu_i_buffer(MAX_IBUFFER_SIZE)
{
  CreateDeviceObjects();
}

VertexManager::~VertexManager()
{
  DestroyDeviceObjects();
}

void VertexManager::CreateDeviceObjects()
{
  m_vertex_buffer = StreamBuffer::Create(GL_ARRAY_BUFFER, MAX_VBUFFER_SIZE);
  m_index_buffer = StreamBuffer::Create(GL_ELEMENT_ARRAY_BUFFER, MAX_IBUFFER_SIZE);
}

void VertexManager::DestroyDeviceObjects()
{
  m_vertex_buffer.reset();
  m_index_buffer.reset();
}

void VertexManager::UploadUtilityUniforms(const void* uniforms, u32 uniforms_size)
{
  ProgramShaderCache::InvalidateConstants();
  ProgramShaderCache::UploadConstants(uniforms, uniforms_size);
}

GLuint VertexManager::GetVertexBufferHandle() const
{
  return m_vertex_buffer->m_buffer;
}

GLuint VertexManager::GetIndexBufferHandle() const
{
  return m_index_buffer->m_buffer;
}

static void CheckBufferBinding()
{
  // The index buffer is part of the VAO state, therefore we need to bind it first.
  if (!ProgramShaderCache::IsValidVertexFormatBound())
  {
    ProgramShaderCache::BindVertexFormat(
        static_cast<GLVertexFormat*>(VertexLoaderManager::GetCurrentVertexFormat()));
  }
}

void VertexManager::ResetBuffer(u32 vertex_stride, bool cull_all)
{
  if (cull_all)
  {
    // This buffer isn't getting sent to the GPU. Just allocate it on the cpu.
    m_cur_buffer_pointer = m_base_buffer_pointer = m_cpu_v_buffer.data();
    m_end_buffer_pointer = m_base_buffer_pointer + m_cpu_v_buffer.size();

    IndexGenerator::Start((u16*)m_cpu_i_buffer.data());
  }
  else
  {
    CheckBufferBinding();

    auto buffer = m_vertex_buffer->Map(MAXVBUFFERSIZE, vertex_stride);
    m_cur_buffer_pointer = m_base_buffer_pointer = buffer.first;
    m_end_buffer_pointer = buffer.first + MAXVBUFFERSIZE;

    buffer = m_index_buffer->Map(MAXIBUFFERSIZE * sizeof(u16));
    IndexGenerator::Start((u16*)buffer.first);
  }
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

  ADDSTAT(stats.thisFrame.bytesVertexStreamed, vertex_data_size);
  ADDSTAT(stats.thisFrame.bytesIndexStreamed, index_data_size);
}

void VertexManager::UploadConstants()
{
  ProgramShaderCache::UploadConstants();
}

void VertexManager::DrawCurrentBatch(u32 base_index, u32 num_indices, u32 base_vertex)
{
  if (::BoundingBox::active && !g_Config.BBoxUseFragmentShaderImplementation())
  {
    glEnable(GL_STENCIL_TEST);
  }

  if (m_current_pipeline_object)
  {
    static_cast<Renderer*>(g_renderer.get())->SetPipeline(m_current_pipeline_object);
    static_cast<Renderer*>(g_renderer.get())->DrawIndexed(base_index, num_indices, base_vertex);
  }

  if (::BoundingBox::active && !g_Config.BBoxUseFragmentShaderImplementation())
  {
    OGL::BoundingBox::StencilWasUpdated();
    glDisable(GL_STENCIL_TEST);
  }

  g_Config.iSaveTargetId++;
  ClearEFBCache();
}
}  // namespace OGL
