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

static std::unique_ptr<StreamBuffer> s_vertexBuffer;
static std::unique_ptr<StreamBuffer> s_indexBuffer;
static size_t s_baseVertex;
static size_t s_index_offset;

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
  s_vertexBuffer = StreamBuffer::Create(GL_ARRAY_BUFFER, MAX_VBUFFER_SIZE);
  m_vertex_buffers = s_vertexBuffer->m_buffer;

  s_indexBuffer = StreamBuffer::Create(GL_ELEMENT_ARRAY_BUFFER, MAX_IBUFFER_SIZE);
  m_index_buffers = s_indexBuffer->m_buffer;
}

void VertexManager::DestroyDeviceObjects()
{
  s_vertexBuffer.reset();
  s_indexBuffer.reset();
}

void VertexManager::PrepareDrawBuffers(u32 stride)
{
  u32 vertex_data_size = IndexGenerator::GetNumVerts() * stride;
  u32 index_data_size = IndexGenerator::GetIndexLen() * sizeof(u16);

  s_vertexBuffer->Unmap(vertex_data_size);
  s_indexBuffer->Unmap(index_data_size);

  ADDSTAT(stats.thisFrame.bytesVertexStreamed, vertex_data_size);
  ADDSTAT(stats.thisFrame.bytesIndexStreamed, index_data_size);
}

void VertexManager::ResetBuffer(u32 stride)
{
  if (m_cull_all)
  {
    // This buffer isn't getting sent to the GPU. Just allocate it on the cpu.
    m_cur_buffer_pointer = m_base_buffer_pointer = m_cpu_v_buffer.data();
    m_end_buffer_pointer = m_base_buffer_pointer + m_cpu_v_buffer.size();

    IndexGenerator::Start((u16*)m_cpu_i_buffer.data());
  }
  else
  {
    auto buffer = s_vertexBuffer->Map(MAXVBUFFERSIZE, stride);
    m_cur_buffer_pointer = m_base_buffer_pointer = buffer.first;
    m_end_buffer_pointer = buffer.first + MAXVBUFFERSIZE;
    s_baseVertex = buffer.second / stride;

    buffer = s_indexBuffer->Map(MAXIBUFFERSIZE * sizeof(u16));
    IndexGenerator::Start((u16*)buffer.first);
    s_index_offset = buffer.second;
  }
}

void VertexManager::Draw(u32 stride)
{
  u32 index_size = IndexGenerator::GetIndexLen();
  u32 max_index = IndexGenerator::GetNumVerts();
  GLenum primitive_mode = 0;

  switch (m_current_primitive_type)
  {
  case PRIMITIVE_POINTS:
    primitive_mode = GL_POINTS;
    glDisable(GL_CULL_FACE);
    break;
  case PRIMITIVE_LINES:
    primitive_mode = GL_LINES;
    glDisable(GL_CULL_FACE);
    break;
  case PRIMITIVE_TRIANGLES:
    primitive_mode =
        g_ActiveConfig.backend_info.bSupportsPrimitiveRestart ? GL_TRIANGLE_STRIP : GL_TRIANGLES;
    break;
  }

  if (g_ogl_config.bSupportsGLBaseVertex)
  {
    glDrawRangeElementsBaseVertex(primitive_mode, 0, max_index, index_size, GL_UNSIGNED_SHORT,
                                  (u8*)nullptr + s_index_offset, (GLint)s_baseVertex);
  }
  else
  {
    glDrawRangeElements(primitive_mode, 0, max_index, index_size, GL_UNSIGNED_SHORT,
                        (u8*)nullptr + s_index_offset);
  }

  INCSTAT(stats.thisFrame.numDrawCalls);

  if (m_current_primitive_type != PRIMITIVE_TRIANGLES)
    static_cast<Renderer*>(g_renderer.get())->SetGenerationMode();
}

void VertexManager::vFlush()
{
  GLVertexFormat* nativeVertexFmt = (GLVertexFormat*)VertexLoaderManager::GetCurrentVertexFormat();
  u32 stride = nativeVertexFmt->GetVertexStride();

  ProgramShaderCache::SetShader(m_current_primitive_type, nativeVertexFmt);

  PrepareDrawBuffers(stride);

  // upload global constants
  ProgramShaderCache::UploadConstants();

  if (::BoundingBox::active && !g_Config.BBoxUseFragmentShaderImplementation())
  {
    glEnable(GL_STENCIL_TEST);
  }

  Draw(stride);

  if (::BoundingBox::active && !g_Config.BBoxUseFragmentShaderImplementation())
  {
    OGL::BoundingBox::StencilWasUpdated();
    glDisable(GL_STENCIL_TEST);
  }

  g_Config.iSaveTargetId++;
  ClearEFBCache();
}

}  // namespace
