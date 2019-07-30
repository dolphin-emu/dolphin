// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D/VertexManager.h"

#include <d3d11.h>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"

#include "VideoBackends/D3D/BoundingBox.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3DCommon/Common.h"

#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{
static ComPtr<ID3D11Buffer> AllocateConstantBuffer(u32 size)
{
  const u32 cbsize = Common::AlignUp(size, 16u);  // must be a multiple of 16
  const CD3D11_BUFFER_DESC cbdesc(cbsize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC,
                                  D3D11_CPU_ACCESS_WRITE);
  ComPtr<ID3D11Buffer> cbuf;
  const HRESULT hr = D3D::device->CreateBuffer(&cbdesc, nullptr, &cbuf);
  CHECK(SUCCEEDED(hr), "shader constant buffer (size=%u)", cbsize);
  if (FAILED(hr))
    return nullptr;

  D3DCommon::SetDebugObjectName(cbuf.Get(), "constant buffer");
  return cbuf;
}

static void UpdateConstantBuffer(ID3D11Buffer* const buffer, const void* data, u32 data_size)
{
  D3D11_MAPPED_SUBRESOURCE map;
  D3D::context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
  memcpy(map.pData, data, data_size);
  D3D::context->Unmap(buffer, 0);

  ADDSTAT(g_stats.this_frame.bytes_uniform_streamed, data_size);
}

static ComPtr<ID3D11ShaderResourceView>
CreateTexelBufferView(ID3D11Buffer* buffer, TexelBufferFormat format, DXGI_FORMAT srv_format)
{
  ComPtr<ID3D11ShaderResourceView> srv;
  CD3D11_SHADER_RESOURCE_VIEW_DESC srv_desc(buffer, srv_format, 0,
                                            VertexManager::TEXEL_STREAM_BUFFER_SIZE /
                                                VertexManager::GetTexelBufferElementSize(format));
  CHECK(SUCCEEDED(D3D::device->CreateShaderResourceView(buffer, &srv_desc, &srv)),
        "Create SRV for texel buffer");
  return srv;
}

VertexManager::VertexManager() = default;

VertexManager::~VertexManager() = default;

bool VertexManager::Initialize()
{
  if (!VertexManagerBase::Initialize())
    return false;

  CD3D11_BUFFER_DESC bufdesc((VERTEX_STREAM_BUFFER_SIZE + INDEX_STREAM_BUFFER_SIZE) / BUFFER_COUNT,
                             D3D11_BIND_INDEX_BUFFER | D3D11_BIND_VERTEX_BUFFER,
                             D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

  for (int i = 0; i < BUFFER_COUNT; i++)
  {
    CHECK(SUCCEEDED(D3D::device->CreateBuffer(&bufdesc, nullptr, &m_buffers[i])),
          "Failed to create buffer.");
    if (m_buffers[i])
      D3DCommon::SetDebugObjectName(m_buffers[i].Get(), "Buffer of VertexManager");
  }

  m_vertex_constant_buffer = AllocateConstantBuffer(sizeof(VertexShaderConstants));
  m_geometry_constant_buffer = AllocateConstantBuffer(sizeof(GeometryShaderConstants));
  m_pixel_constant_buffer = AllocateConstantBuffer(sizeof(PixelShaderConstants));
  if (!m_vertex_constant_buffer || !m_geometry_constant_buffer || !m_pixel_constant_buffer)
    return false;

  CD3D11_BUFFER_DESC texel_buf_desc(TEXEL_STREAM_BUFFER_SIZE, D3D11_BIND_SHADER_RESOURCE,
                                    D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
  CHECK(SUCCEEDED(D3D::device->CreateBuffer(&texel_buf_desc, nullptr, &m_texel_buffer)),
        "Creating texel buffer failed");
  if (!m_texel_buffer)
    return false;

  static constexpr std::array<std::pair<TexelBufferFormat, DXGI_FORMAT>, NUM_TEXEL_BUFFER_FORMATS>
      format_mapping = {{
          {TEXEL_BUFFER_FORMAT_R8_UINT, DXGI_FORMAT_R8_UINT},
          {TEXEL_BUFFER_FORMAT_R16_UINT, DXGI_FORMAT_R16_UINT},
          {TEXEL_BUFFER_FORMAT_RGBA8_UINT, DXGI_FORMAT_R8G8B8A8_UINT},
          {TEXEL_BUFFER_FORMAT_R32G32_UINT, DXGI_FORMAT_R32G32_UINT},
      }};
  for (const auto& it : format_mapping)
  {
    m_texel_buffer_views[it.first] =
        CreateTexelBufferView(m_texel_buffer.Get(), it.first, it.second);
    if (!m_texel_buffer_views[it.first])
      return false;
  }

  return true;
}

void VertexManager::UploadUtilityUniforms(const void* uniforms, u32 uniforms_size)
{
  // Just use the one buffer for all three.
  InvalidateConstants();
  UpdateConstantBuffer(m_vertex_constant_buffer.Get(), uniforms, uniforms_size);
  D3D::stateman->SetVertexConstants(m_vertex_constant_buffer.Get());
  D3D::stateman->SetGeometryConstants(m_vertex_constant_buffer.Get());
  D3D::stateman->SetPixelConstants(m_vertex_constant_buffer.Get());
}

bool VertexManager::MapTexelBuffer(u32 required_size, D3D11_MAPPED_SUBRESOURCE& sr)
{
  if ((m_texel_buffer_offset + required_size) > TEXEL_STREAM_BUFFER_SIZE)
  {
    // Restart buffer.
    HRESULT hr = D3D::context->Map(m_texel_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &sr);
    CHECK(SUCCEEDED(hr), "Map texel buffer");
    if (FAILED(hr))
      return false;

    m_texel_buffer_offset = 0;
  }
  else
  {
    // Don't overwrite the earlier-used space.
    HRESULT hr = D3D::context->Map(m_texel_buffer.Get(), 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &sr);
    CHECK(SUCCEEDED(hr), "Map texel buffer");
    if (FAILED(hr))
      return false;
  }

  return true;
}

bool VertexManager::UploadTexelBuffer(const void* data, u32 data_size, TexelBufferFormat format,
                                      u32* out_offset)
{
  if (data_size > TEXEL_STREAM_BUFFER_SIZE)
    return false;

  const u32 elem_size = GetTexelBufferElementSize(format);
  m_texel_buffer_offset = Common::AlignUp(m_texel_buffer_offset, elem_size);

  D3D11_MAPPED_SUBRESOURCE sr;
  if (!MapTexelBuffer(data_size, sr))
    return false;

  *out_offset = m_texel_buffer_offset / elem_size;
  std::memcpy(static_cast<u8*>(sr.pData) + m_texel_buffer_offset, data, data_size);
  ADDSTAT(g_stats.this_frame.bytes_uniform_streamed, data_size);
  m_texel_buffer_offset += data_size;

  D3D::context->Unmap(m_texel_buffer.Get(), 0);
  D3D::stateman->SetTexture(0, m_texel_buffer_views[static_cast<size_t>(format)].Get());
  return true;
}

bool VertexManager::UploadTexelBuffer(const void* data, u32 data_size, TexelBufferFormat format,
                                      u32* out_offset, const void* palette_data, u32 palette_size,
                                      TexelBufferFormat palette_format, u32* out_palette_offset)
{
  const u32 elem_size = GetTexelBufferElementSize(format);
  const u32 palette_elem_size = GetTexelBufferElementSize(palette_format);
  const u32 reserve_size = data_size + palette_size + palette_elem_size;
  if (reserve_size > TEXEL_STREAM_BUFFER_SIZE)
    return false;

  m_texel_buffer_offset = Common::AlignUp(m_texel_buffer_offset, elem_size);

  D3D11_MAPPED_SUBRESOURCE sr;
  if (!MapTexelBuffer(reserve_size, sr))
    return false;

  const u32 palette_byte_offset = Common::AlignUp(data_size, palette_elem_size);
  std::memcpy(static_cast<u8*>(sr.pData) + m_texel_buffer_offset, data, data_size);
  std::memcpy(static_cast<u8*>(sr.pData) + m_texel_buffer_offset + palette_byte_offset,
              palette_data, palette_size);
  ADDSTAT(g_stats.this_frame.bytes_uniform_streamed, palette_byte_offset + palette_size);
  *out_offset = m_texel_buffer_offset / elem_size;
  *out_palette_offset = (m_texel_buffer_offset + palette_byte_offset) / palette_elem_size;
  m_texel_buffer_offset += palette_byte_offset + palette_size;

  D3D::context->Unmap(m_texel_buffer.Get(), 0);
  D3D::stateman->SetTexture(0, m_texel_buffer_views[static_cast<size_t>(format)].Get());
  D3D::stateman->SetTexture(1, m_texel_buffer_views[static_cast<size_t>(palette_format)].Get());
  return true;
}

void VertexManager::ResetBuffer(u32 vertex_stride)
{
  m_base_buffer_pointer = m_cpu_vertex_buffer.data();
  m_cur_buffer_pointer = m_base_buffer_pointer;
  m_end_buffer_pointer = m_base_buffer_pointer + m_cpu_vertex_buffer.size();
  IndexGenerator::Start(m_cpu_index_buffer.data());
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
  if (cursor + totalBufferSize >= BUFFER_SIZE)
  {
    // Wrap around
    m_current_buffer = (m_current_buffer + 1) % BUFFER_COUNT;
    cursor = 0;
    MapType = D3D11_MAP_WRITE_DISCARD;
  }

  *out_base_vertex = vertex_stride > 0 ? (cursor / vertex_stride) : 0;
  *out_base_index = (cursor + vertexBufferSize) / sizeof(u16);

  D3D::context->Map(m_buffers[m_current_buffer].Get(), 0, MapType, 0, &map);
  u8* mappedData = reinterpret_cast<u8*>(map.pData);
  if (vertexBufferSize > 0)
    std::memcpy(mappedData + cursor, m_base_buffer_pointer, vertexBufferSize);
  if (indexBufferSize > 0)
    std::memcpy(mappedData + cursor + vertexBufferSize, m_cpu_index_buffer.data(), indexBufferSize);
  D3D::context->Unmap(m_buffers[m_current_buffer].Get(), 0);

  m_buffer_cursor = cursor + totalBufferSize;

  ADDSTAT(g_stats.this_frame.bytes_vertex_streamed, vertexBufferSize);
  ADDSTAT(g_stats.this_frame.bytes_index_streamed, indexBufferSize);

  D3D::stateman->SetVertexBuffer(m_buffers[m_current_buffer].Get(), vertex_stride, 0);
  D3D::stateman->SetIndexBuffer(m_buffers[m_current_buffer].Get());
}

void VertexManager::UploadUniforms()
{
  if (VertexShaderManager::dirty)
  {
    UpdateConstantBuffer(m_vertex_constant_buffer.Get(), &VertexShaderManager::constants,
                         sizeof(VertexShaderConstants));
    VertexShaderManager::dirty = false;
  }
  if (GeometryShaderManager::dirty)
  {
    UpdateConstantBuffer(m_geometry_constant_buffer.Get(), &GeometryShaderManager::constants,
                         sizeof(GeometryShaderConstants));
    GeometryShaderManager::dirty = false;
  }
  if (PixelShaderManager::dirty)
  {
    UpdateConstantBuffer(m_pixel_constant_buffer.Get(), &PixelShaderManager::constants,
                         sizeof(PixelShaderConstants));
    PixelShaderManager::dirty = false;
  }

  D3D::stateman->SetPixelConstants(
      m_pixel_constant_buffer.Get(),
      g_ActiveConfig.bEnablePixelLighting ? m_vertex_constant_buffer.Get() : nullptr);
  D3D::stateman->SetVertexConstants(m_vertex_constant_buffer.Get());
  D3D::stateman->SetGeometryConstants(m_geometry_constant_buffer.Get());
}
}  // namespace DX11
