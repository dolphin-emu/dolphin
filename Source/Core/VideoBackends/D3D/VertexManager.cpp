// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D/VertexManager.h"

#include <d3d11.h>

#include "Common/Align.h"
#include "Common/CommonTypes.h"

#include "VideoBackends/D3D/BoundingBox.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/FramebufferManager.h"
#include "VideoBackends/D3D/GeometryShaderCache.h"
#include "VideoBackends/D3D/PixelShaderCache.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3D/VertexShaderCache.h"

#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/Debugger.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{
// TODO: Find sensible values for these two
const u32 MAX_IBUFFER_SIZE = VertexManager::MAXIBUFFERSIZE * sizeof(u16) * 8;
const u32 MAX_VBUFFER_SIZE = VertexManager::MAXVBUFFERSIZE;
const u32 MAX_BUFFER_SIZE = MAX_IBUFFER_SIZE + MAX_VBUFFER_SIZE;

static ID3D11Buffer* AllocateConstantBuffer(u32 size)
{
  const u32 cbsize = Common::AlignUp(size, 16u);  // must be a multiple of 16
  const CD3D11_BUFFER_DESC cbdesc(cbsize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC,
                                  D3D11_CPU_ACCESS_WRITE);
  ID3D11Buffer* cbuf;
  const HRESULT hr = D3D::device->CreateBuffer(&cbdesc, nullptr, &cbuf);
  CHECK(hr == S_OK, "shader constant buffer (size=%u)", cbsize);
  D3D::SetDebugObjectName(cbuf, "constant buffer used to emulate the GX pipeline");
  return cbuf;
}

static void UpdateConstantBuffer(ID3D11Buffer* const buffer, const void* data, u32 data_size)
{
  D3D11_MAPPED_SUBRESOURCE map;
  D3D::context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
  memcpy(map.pData, data, data_size);
  D3D::context->Unmap(buffer, 0);

  ADDSTAT(stats.thisFrame.bytesUniformStreamed, data_size);
}

void VertexManager::CreateDeviceObjects()
{
  D3D11_BUFFER_DESC bufdesc =
      CD3D11_BUFFER_DESC(MAX_BUFFER_SIZE, D3D11_BIND_INDEX_BUFFER | D3D11_BIND_VERTEX_BUFFER,
                         D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

  for (int i = 0; i < MAX_BUFFER_COUNT; i++)
  {
    m_buffers[i] = nullptr;
    CHECK(SUCCEEDED(D3D::device->CreateBuffer(&bufdesc, nullptr, &m_buffers[i])),
          "Failed to create buffer.");
    D3D::SetDebugObjectName(m_buffers[i], "Buffer of VertexManager");
  }

  m_buffer_cursor = MAX_BUFFER_SIZE;

  m_vertex_constant_buffer = AllocateConstantBuffer(sizeof(VertexShaderConstants));
  m_geometry_constant_buffer = AllocateConstantBuffer(sizeof(GeometryShaderConstants));
  m_pixel_constant_buffer = AllocateConstantBuffer(sizeof(PixelShaderConstants));
}

void VertexManager::DestroyDeviceObjects()
{
  SAFE_RELEASE(m_pixel_constant_buffer);
  SAFE_RELEASE(m_geometry_constant_buffer);
  SAFE_RELEASE(m_vertex_constant_buffer);
  for (int i = 0; i < MAX_BUFFER_COUNT; i++)
  {
    SAFE_RELEASE(m_buffers[i]);
  }
}

VertexManager::VertexManager()
{
  m_staging_vertex_buffer.resize(MAXVBUFFERSIZE);

  m_cur_buffer_pointer = m_base_buffer_pointer = &m_staging_vertex_buffer[0];
  m_end_buffer_pointer = m_base_buffer_pointer + m_staging_vertex_buffer.size();

  m_staging_index_buffer.resize(MAXIBUFFERSIZE);

  CreateDeviceObjects();
}

VertexManager::~VertexManager()
{
  DestroyDeviceObjects();
}

void VertexManager::UploadUtilityUniforms(const void* uniforms, u32 uniforms_size)
{
  // Just use the one buffer for all three.
  UpdateConstantBuffer(m_vertex_constant_buffer, uniforms, uniforms_size);
  D3D::stateman->SetVertexConstants(m_vertex_constant_buffer);
  D3D::stateman->SetGeometryConstants(m_vertex_constant_buffer);
  D3D::stateman->SetPixelConstants(m_vertex_constant_buffer);
  VertexShaderManager::dirty = true;
  GeometryShaderManager::dirty = true;
  PixelShaderManager::dirty = true;
}

void VertexManager::ResetBuffer(u32 vertex_stride, bool cull_all)
{
  m_cur_buffer_pointer = m_base_buffer_pointer;
  IndexGenerator::Start(m_staging_index_buffer.data());
}

void VertexManager::CommitBuffer(u32 num_vertices, u32 vertex_stride, u32 num_indices,
                                 u32* out_base_vertex, u32* out_base_index)
{
  D3D11_MAPPED_SUBRESOURCE map;

  u32 vertexBufferSize = Common::AlignUp(num_vertices * vertex_stride, sizeof(u16));
  u32 indexBufferSize = num_indices * sizeof(u16);
  u32 totalBufferSize = vertexBufferSize + indexBufferSize;

  u32 cursor = m_buffer_cursor;
  u32 padding = vertex_stride > 0 ? (m_buffer_cursor % vertex_stride) : 0;
  if (padding)
  {
    cursor += vertex_stride - padding;
  }

  D3D11_MAP MapType = D3D11_MAP_WRITE_NO_OVERWRITE;
  if (cursor + totalBufferSize >= MAX_BUFFER_SIZE)
  {
    // Wrap around
    m_current_buffer = (m_current_buffer + 1) % MAX_BUFFER_COUNT;
    cursor = 0;
    MapType = D3D11_MAP_WRITE_DISCARD;
  }

  *out_base_vertex = vertex_stride > 0 ? (cursor / vertex_stride) : 0;
  *out_base_index = (cursor + vertexBufferSize) / sizeof(u16);

  D3D::context->Map(m_buffers[m_current_buffer], 0, MapType, 0, &map);
  u8* mappedData = reinterpret_cast<u8*>(map.pData);
  if (vertexBufferSize > 0)
    std::memcpy(mappedData + cursor, m_base_buffer_pointer, vertexBufferSize);
  if (indexBufferSize > 0)
    std::memcpy(mappedData + cursor + vertexBufferSize, m_staging_index_buffer.data(),
                indexBufferSize);
  D3D::context->Unmap(m_buffers[m_current_buffer], 0);

  m_buffer_cursor = cursor + totalBufferSize;

  ADDSTAT(stats.thisFrame.bytesVertexStreamed, vertexBufferSize);
  ADDSTAT(stats.thisFrame.bytesIndexStreamed, indexBufferSize);

  D3D::stateman->SetVertexBuffer(m_buffers[m_current_buffer], vertex_stride, 0);
  D3D::stateman->SetIndexBuffer(m_buffers[m_current_buffer]);
}

void VertexManager::UploadConstants()
{
  if (VertexShaderManager::dirty)
  {
    UpdateConstantBuffer(m_vertex_constant_buffer, &VertexShaderManager::constants,
                         sizeof(VertexShaderConstants));
    VertexShaderManager::dirty = false;
  }
  if (GeometryShaderManager::dirty)
  {
    UpdateConstantBuffer(m_geometry_constant_buffer, &GeometryShaderManager::constants,
                         sizeof(GeometryShaderConstants));
    GeometryShaderManager::dirty = false;
  }
  if (PixelShaderManager::dirty)
  {
    UpdateConstantBuffer(m_pixel_constant_buffer, &PixelShaderManager::constants,
                         sizeof(PixelShaderConstants));
    PixelShaderManager::dirty = false;
  }

  D3D::stateman->SetPixelConstants(m_pixel_constant_buffer, g_ActiveConfig.bEnablePixelLighting ?
                                                                m_vertex_constant_buffer :
                                                                nullptr);
  D3D::stateman->SetVertexConstants(m_vertex_constant_buffer);
  D3D::stateman->SetGeometryConstants(m_geometry_constant_buffer);
}

void VertexManager::DrawCurrentBatch(u32 base_index, u32 num_indices, u32 base_vertex)
{
  FramebufferManager::SetIntegerEFBRenderTarget(
      m_current_pipeline_config.blending_state.logicopenable);

  if (g_ActiveConfig.backend_info.bSupportsBBox && BoundingBox::active)
  {
    D3D::context->OMSetRenderTargetsAndUnorderedAccessViews(
        D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, nullptr, nullptr, 2, 1, &BBox::GetUAV(),
        nullptr);
  }

  D3D::stateman->Apply();
  D3D::context->DrawIndexed(num_indices, base_index, base_vertex);
}
}  // namespace DX11
