// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D/VertexManager.h"

#include <d3d11.h>

#include "Common/CommonTypes.h"

#include "VideoBackends/D3D/BoundingBox.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/GeometryShaderCache.h"
#include "VideoBackends/D3D/PixelShaderCache.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3D/VertexShaderCache.h"

#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/Debugger.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{
// TODO: Find sensible values for these two
const u32 MAX_IBUFFER_SIZE = VertexManager::MAXIBUFFERSIZE * sizeof(u16) * 8;
const u32 MAX_VBUFFER_SIZE = VertexManager::MAXVBUFFERSIZE;
const u32 MAX_BUFFER_SIZE = MAX_IBUFFER_SIZE + MAX_VBUFFER_SIZE;

void VertexManager::CreateDeviceObjects()
{
  D3D11_BUFFER_DESC bufdesc =
      CD3D11_BUFFER_DESC(MAX_BUFFER_SIZE, D3D11_BIND_INDEX_BUFFER | D3D11_BIND_VERTEX_BUFFER,
                         D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

  m_vertexDrawOffset = 0;
  m_indexDrawOffset = 0;

  for (int i = 0; i < MAX_BUFFER_COUNT; i++)
  {
    m_buffers[i] = nullptr;
    CHECK(SUCCEEDED(D3D::device->CreateBuffer(&bufdesc, nullptr, &m_buffers[i])),
          "Failed to create buffer.");
    D3D::SetDebugObjectName((ID3D11DeviceChild*)m_buffers[i], "Buffer of VertexManager");
  }

  m_currentBuffer = 0;
  m_bufferCursor = MAX_BUFFER_SIZE;
}

void VertexManager::DestroyDeviceObjects()
{
  for (int i = 0; i < MAX_BUFFER_COUNT; i++)
  {
    SAFE_RELEASE(m_buffers[i]);
  }
}

VertexManager::VertexManager()
{
  LocalVBuffer.resize(MAXVBUFFERSIZE);

  m_cur_buffer_pointer = m_base_buffer_pointer = &LocalVBuffer[0];
  m_end_buffer_pointer = m_base_buffer_pointer + LocalVBuffer.size();

  LocalIBuffer.resize(MAXIBUFFERSIZE);

  CreateDeviceObjects();
}

VertexManager::~VertexManager()
{
  DestroyDeviceObjects();
}

void VertexManager::PrepareDrawBuffers(u32 stride)
{
  D3D11_MAPPED_SUBRESOURCE map;

  u32 vertexBufferSize = u32(m_cur_buffer_pointer - m_base_buffer_pointer);
  u32 indexBufferSize = IndexGenerator::GetIndexLen() * sizeof(u16);
  u32 totalBufferSize = vertexBufferSize + indexBufferSize;

  u32 cursor = m_bufferCursor;
  u32 padding = m_bufferCursor % stride;
  if (padding)
  {
    cursor += stride - padding;
  }

  D3D11_MAP MapType = D3D11_MAP_WRITE_NO_OVERWRITE;
  if (cursor + totalBufferSize >= MAX_BUFFER_SIZE)
  {
    // Wrap around
    m_currentBuffer = (m_currentBuffer + 1) % MAX_BUFFER_COUNT;
    cursor = 0;
    MapType = D3D11_MAP_WRITE_DISCARD;
  }

  m_vertexDrawOffset = cursor;
  m_indexDrawOffset = cursor + vertexBufferSize;

  D3D::context->Map(m_buffers[m_currentBuffer], 0, MapType, 0, &map);
  u8* mappedData = reinterpret_cast<u8*>(map.pData);
  memcpy(mappedData + m_vertexDrawOffset, m_base_buffer_pointer, vertexBufferSize);
  memcpy(mappedData + m_indexDrawOffset, GetIndexBuffer(), indexBufferSize);
  D3D::context->Unmap(m_buffers[m_currentBuffer], 0);

  m_bufferCursor = cursor + totalBufferSize;

  ADDSTAT(stats.thisFrame.bytesVertexStreamed, vertexBufferSize);
  ADDSTAT(stats.thisFrame.bytesIndexStreamed, indexBufferSize);
}

void VertexManager::Draw(u32 stride)
{
  u32 indices = IndexGenerator::GetIndexLen();

  D3D::stateman->SetVertexBuffer(m_buffers[m_currentBuffer], stride, 0);
  D3D::stateman->SetIndexBuffer(m_buffers[m_currentBuffer]);

  u32 baseVertex = m_vertexDrawOffset / stride;
  u32 startIndex = m_indexDrawOffset / sizeof(u16);

  switch (m_current_primitive_type)
  {
  case PRIMITIVE_POINTS:
    D3D::stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    static_cast<Renderer*>(g_renderer.get())->ApplyCullDisable();
    break;
  case PRIMITIVE_LINES:
    D3D::stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    static_cast<Renderer*>(g_renderer.get())->ApplyCullDisable();
    break;
  case PRIMITIVE_TRIANGLES:
    D3D::stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    break;
  }

  D3D::stateman->Apply();
  D3D::context->DrawIndexed(indices, startIndex, baseVertex);

  INCSTAT(stats.thisFrame.numDrawCalls);

  if (m_current_primitive_type != PRIMITIVE_TRIANGLES)
    static_cast<Renderer*>(g_renderer.get())->RestoreCull();
}

void VertexManager::vFlush()
{
  if (!PixelShaderCache::SetShader())
  {
    GFX_DEBUGGER_PAUSE_LOG_AT(NEXT_ERROR, true, { printf("Fail to set pixel shader\n"); });
    return;
  }

  D3DVertexFormat* vertex_format =
      static_cast<D3DVertexFormat*>(VertexLoaderManager::GetCurrentVertexFormat());
  if (!VertexShaderCache::SetShader(vertex_format))
  {
    GFX_DEBUGGER_PAUSE_LOG_AT(NEXT_ERROR, true, { printf("Fail to set pixel shader\n"); });
    return;
  }

  if (!GeometryShaderCache::SetShader(m_current_primitive_type))
  {
    GFX_DEBUGGER_PAUSE_LOG_AT(NEXT_ERROR, true, { printf("Fail to set pixel shader\n"); });
    return;
  }

  if (g_ActiveConfig.backend_info.bSupportsBBox && BoundingBox::active)
  {
    D3D::context->OMSetRenderTargetsAndUnorderedAccessViews(
        D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, nullptr, nullptr, 2, 1, &BBox::GetUAV(),
        nullptr);
  }

  u32 stride = VertexLoaderManager::GetCurrentVertexFormat()->GetVertexStride();

  PrepareDrawBuffers(stride);

  g_renderer->ApplyState();

  Draw(stride);

  g_renderer->RestoreState();
}

void VertexManager::ResetBuffer(u32 stride)
{
  m_cur_buffer_pointer = m_base_buffer_pointer;
  IndexGenerator::Start(GetIndexBuffer());
}

}  // namespace
