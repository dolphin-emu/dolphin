// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D/D3DUtil.h"

#include <cctype>
#include <list>
#include <string>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DShader.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/GeometryShaderCache.h"
#include "VideoBackends/D3D/PixelShaderCache.h"
#include "VideoBackends/D3D/VertexShaderCache.h"
#include "VideoCommon/VideoBackendBase.h"

namespace DX11
{
namespace D3D
{
// Ring buffer class, shared between the draw* functions
class UtilVertexBuffer
{
public:
  UtilVertexBuffer(unsigned int size) : max_size(size)
  {
    D3D11_BUFFER_DESC desc = CD3D11_BUFFER_DESC(max_size, D3D11_BIND_VERTEX_BUFFER,
                                                D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
    device->CreateBuffer(&desc, nullptr, &buf);
  }
  ~UtilVertexBuffer() { buf->Release(); }
  int GetSize() const { return max_size; }
  // returns vertex offset to the new data
  int AppendData(void* data, unsigned int size, unsigned int vertex_size)
  {
    D3D11_MAPPED_SUBRESOURCE map;
    if (offset + size >= max_size)
    {
      // wrap buffer around and notify observers
      offset = 0;
      context->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);

      for (bool* observer : observers)
        *observer = true;
    }
    else
    {
      context->Map(buf, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &map);
    }
    offset = Common::AlignUp(offset, vertex_size);
    memcpy((u8*)map.pData + offset, data, size);
    context->Unmap(buf, 0);

    offset += size;
    return (offset - size) / vertex_size;
  }

  int BeginAppendData(void** write_ptr, unsigned int size, unsigned int vertex_size)
  {
    DEBUG_ASSERT(size < max_size);

    D3D11_MAPPED_SUBRESOURCE map;
    unsigned int aligned_offset = Common::AlignUp(offset, vertex_size);
    if (aligned_offset + size > max_size)
    {
      // wrap buffer around and notify observers
      offset = 0;
      aligned_offset = 0;
      context->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);

      for (bool* observer : observers)
        *observer = true;
    }
    else
    {
      context->Map(buf, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &map);
    }

    *write_ptr = reinterpret_cast<byte*>(map.pData) + aligned_offset;
    offset = aligned_offset + size;
    return aligned_offset / vertex_size;
  }

  void EndAppendData() { context->Unmap(buf, 0); }
  void AddWrapObserver(bool* observer) { observers.push_back(observer); }
  inline ID3D11Buffer*& GetBuffer() { return buf; }

private:
  ID3D11Buffer* buf = nullptr;
  unsigned int offset = 0;
  unsigned int max_size;

  std::list<bool*> observers;
};

static UtilVertexBuffer* util_vbuf = nullptr;
static ID3D11SamplerState* linear_copy_sampler = nullptr;
static ID3D11SamplerState* point_copy_sampler = nullptr;

struct STQVertex
{
  float x, y, z, u, v, w;
};
struct ClearVertex
{
  float x, y, z;
  u32 col;
};
struct ColVertex
{
  float x, y, z;
  u32 col;
};

struct TexQuadData
{
  float u1, v1, u2, v2, S, G;
};
static TexQuadData tex_quad_data;

struct DrawQuadData
{
  float x1, y1, x2, y2, z;
  u32 col;
};
static DrawQuadData draw_quad_data;

struct ClearQuadData
{
  u32 col;
  float z;
};
static ClearQuadData clear_quad_data;

// ring buffer offsets
static int stq_offset, cq_offset, clearq_offset;

// observer variables for ring buffer wraps
static bool stq_observer, cq_observer, clearq_observer;

void InitUtils()
{
  util_vbuf = new UtilVertexBuffer(65536);  // 64KiB

  float border[4] = {0.f, 0.f, 0.f, 0.f};
  D3D11_SAMPLER_DESC samDesc = CD3D11_SAMPLER_DESC(
      D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_BORDER,
      D3D11_TEXTURE_ADDRESS_BORDER, 0.f, 1, D3D11_COMPARISON_ALWAYS, border, 0.f, 0.f);
  HRESULT hr = D3D::device->CreateSamplerState(&samDesc, &point_copy_sampler);
  if (FAILED(hr))
    PanicAlert("Failed to create sampler state at %s %d\n", __FILE__, __LINE__);
  else
    SetDebugObjectName(point_copy_sampler, "point copy sampler state");

  samDesc = CD3D11_SAMPLER_DESC(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_BORDER,
                                D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_BORDER, 0.f, 1,
                                D3D11_COMPARISON_ALWAYS, border, 0.f, 0.f);
  hr = D3D::device->CreateSamplerState(&samDesc, &linear_copy_sampler);
  if (FAILED(hr))
    PanicAlert("Failed to create sampler state at %s %d\n", __FILE__, __LINE__);
  else
    SetDebugObjectName(linear_copy_sampler, "linear copy sampler state");

  // cached data used to avoid unnecessarily reloading the vertex buffers
  memset(&tex_quad_data, 0, sizeof(tex_quad_data));
  memset(&draw_quad_data, 0, sizeof(draw_quad_data));
  memset(&clear_quad_data, 0, sizeof(clear_quad_data));

  // make sure to properly load the vertex data whenever the corresponding functions get called the
  // first time
  stq_observer = cq_observer = clearq_observer = true;
  util_vbuf->AddWrapObserver(&stq_observer);
  util_vbuf->AddWrapObserver(&cq_observer);
  util_vbuf->AddWrapObserver(&clearq_observer);
}

void ShutdownUtils()
{
  SAFE_RELEASE(point_copy_sampler);
  SAFE_RELEASE(linear_copy_sampler);
  SAFE_DELETE(util_vbuf);
}

void SetPointCopySampler()
{
  D3D::stateman->SetSampler(0, point_copy_sampler);
}

void SetLinearCopySampler()
{
  D3D::stateman->SetSampler(0, linear_copy_sampler);
}

void drawShadedTexQuad(ID3D11ShaderResourceView* texture, const D3D11_RECT* rSource,
                       int SourceWidth, int SourceHeight, ID3D11PixelShader* PShader,
                       ID3D11VertexShader* VShader, ID3D11InputLayout* layout,
                       ID3D11GeometryShader* GShader, u32 slice)
{
  float sw = 1.0f / (float)SourceWidth;
  float sh = 1.0f / (float)SourceHeight;
  float u1 = ((float)rSource->left) * sw;
  float u2 = ((float)rSource->right) * sw;
  float v1 = ((float)rSource->top) * sh;
  float v2 = ((float)rSource->bottom) * sh;
  float S = (float)slice;

  STQVertex coords[4] = {
      {-1.0f, 1.0f, 0.0f, u1, v1, S},
      {1.0f, 1.0f, 0.0f, u2, v1, S},
      {-1.0f, -1.0f, 0.0f, u1, v2, S},
      {1.0f, -1.0f, 0.0f, u2, v2, S},
  };

  // only upload the data to VRAM if it changed
  if (stq_observer || tex_quad_data.u1 != u1 || tex_quad_data.v1 != v1 || tex_quad_data.u2 != u2 ||
      tex_quad_data.v2 != v2 || tex_quad_data.S != S)
  {
    stq_offset = util_vbuf->AppendData(coords, sizeof(coords), sizeof(STQVertex));
    stq_observer = false;

    tex_quad_data.u1 = u1;
    tex_quad_data.v1 = v1;
    tex_quad_data.u2 = u2;
    tex_quad_data.v2 = v2;
    tex_quad_data.S = S;
  }
  UINT stride = sizeof(STQVertex);
  UINT offset = 0;

  D3D::stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  D3D::stateman->SetInputLayout(layout);
  D3D::stateman->SetVertexBuffer(util_vbuf->GetBuffer(), stride, offset);
  D3D::stateman->SetPixelShader(PShader);
  D3D::stateman->SetTexture(0, texture);
  D3D::stateman->SetVertexShader(VShader);
  D3D::stateman->SetGeometryShader(GShader);

  D3D::stateman->Apply();
  D3D::context->Draw(4, stq_offset);

  D3D::stateman->SetTexture(0, nullptr);  // immediately unbind the texture
  D3D::stateman->Apply();

  D3D::stateman->SetGeometryShader(nullptr);
}

// Fills a certain area of the current render target with the specified color
// destination coordinates normalized to (-1;1)
void drawColorQuad(u32 Color, float z, float x1, float y1, float x2, float y2)
{
  ColVertex coords[4] = {
      {x1, y1, z, Color},
      {x2, y1, z, Color},
      {x1, y2, z, Color},
      {x2, y2, z, Color},
  };

  if (cq_observer || draw_quad_data.x1 != x1 || draw_quad_data.y1 != y1 ||
      draw_quad_data.x2 != x2 || draw_quad_data.y2 != y2 || draw_quad_data.col != Color ||
      draw_quad_data.z != z)
  {
    cq_offset = util_vbuf->AppendData(coords, sizeof(coords), sizeof(ColVertex));
    cq_observer = false;

    draw_quad_data.x1 = x1;
    draw_quad_data.y1 = y1;
    draw_quad_data.x2 = x2;
    draw_quad_data.y2 = y2;
    draw_quad_data.col = Color;
    draw_quad_data.z = z;
  }

  stateman->SetVertexShader(VertexShaderCache::GetClearVertexShader());
  stateman->SetGeometryShader(GeometryShaderCache::GetClearGeometryShader());
  stateman->SetPixelShader(PixelShaderCache::GetClearProgram());
  stateman->SetInputLayout(VertexShaderCache::GetClearInputLayout());

  UINT stride = sizeof(ColVertex);
  UINT offset = 0;
  stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  stateman->SetVertexBuffer(util_vbuf->GetBuffer(), stride, offset);

  stateman->Apply();
  context->Draw(4, cq_offset);

  stateman->SetGeometryShader(nullptr);
}

void drawClearQuad(u32 Color, float z)
{
  ClearVertex coords[4] = {
      {-1.0f, 1.0f, z, Color},
      {1.0f, 1.0f, z, Color},
      {-1.0f, -1.0f, z, Color},
      {1.0f, -1.0f, z, Color},
  };

  if (clearq_observer || clear_quad_data.col != Color || clear_quad_data.z != z)
  {
    clearq_offset = util_vbuf->AppendData(coords, sizeof(coords), sizeof(ClearVertex));
    clearq_observer = false;

    clear_quad_data.col = Color;
    clear_quad_data.z = z;
  }

  stateman->SetVertexShader(VertexShaderCache::GetClearVertexShader());
  stateman->SetGeometryShader(GeometryShaderCache::GetClearGeometryShader());
  stateman->SetPixelShader(PixelShaderCache::GetClearProgram());
  stateman->SetInputLayout(VertexShaderCache::GetClearInputLayout());

  UINT stride = sizeof(ClearVertex);
  UINT offset = 0;
  stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  stateman->SetVertexBuffer(util_vbuf->GetBuffer(), stride, offset);

  stateman->Apply();
  context->Draw(4, clearq_offset);

  stateman->SetGeometryShader(nullptr);
}

static void InitColVertex(ColVertex* vert, float x, float y, float z, u32 col)
{
  vert->x = x;
  vert->y = y;
  vert->z = z;
  vert->col = col;
}

void DrawEFBPokeQuads(EFBAccessType type, const EfbPokeData* points, size_t num_points)
{
  const size_t COL_QUAD_SIZE = sizeof(ColVertex) * 6;

  // Set common state
  stateman->SetVertexShader(VertexShaderCache::GetClearVertexShader());
  stateman->SetGeometryShader(GeometryShaderCache::GetClearGeometryShader());
  stateman->SetPixelShader(PixelShaderCache::GetClearProgram());
  stateman->SetInputLayout(VertexShaderCache::GetClearInputLayout());
  stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  stateman->SetVertexBuffer(util_vbuf->GetBuffer(), sizeof(ColVertex), 0);
  stateman->Apply();

  // if drawing a large number of points at once, this will have to be split into multiple passes.
  size_t points_per_draw = util_vbuf->GetSize() / COL_QUAD_SIZE;
  size_t current_point_index = 0;
  while (current_point_index < num_points)
  {
    size_t points_to_draw = std::min(num_points - current_point_index, points_per_draw);
    size_t required_bytes = COL_QUAD_SIZE * points_to_draw;

    // map and reserve enough buffer space for this draw
    void* buffer_ptr;
    int base_vertex_index =
        util_vbuf->BeginAppendData(&buffer_ptr, (int)required_bytes, sizeof(ColVertex));

    // generate quads for each efb point
    ColVertex* base_vertex_ptr = reinterpret_cast<ColVertex*>(buffer_ptr);
    for (size_t i = 0; i < points_to_draw; i++)
    {
      // generate quad from the single point (clip-space coordinates)
      const EfbPokeData* point = &points[current_point_index];
      float x1 = float(point->x) * 2.0f / EFB_WIDTH - 1.0f;
      float y1 = -float(point->y) * 2.0f / EFB_HEIGHT + 1.0f;
      float x2 = float(point->x + 1) * 2.0f / EFB_WIDTH - 1.0f;
      float y2 = -float(point->y + 1) * 2.0f / EFB_HEIGHT + 1.0f;
      float z = 0.0f;
      u32 col = 0;

      if (type == EFBAccessType::PokeZ)
      {
        z = 1.0f - static_cast<float>(point->data & 0xFFFFFF) / 16777216.0f;
      }
      else
      {
        col = ((point->data & 0xFF00FF00) | ((point->data >> 16) & 0xFF) |
               ((point->data << 16) & 0xFF0000));
      }

      current_point_index++;

      // quad -> triangles
      ColVertex* vertex = &base_vertex_ptr[i * 6];
      InitColVertex(&vertex[0], x1, y1, z, col);
      InitColVertex(&vertex[1], x2, y1, z, col);
      InitColVertex(&vertex[2], x1, y2, z, col);
      InitColVertex(&vertex[3], x1, y2, z, col);
      InitColVertex(&vertex[4], x2, y1, z, col);
      InitColVertex(&vertex[5], x2, y2, z, col);
    }

    // unmap the util buffer, and issue the draw
    util_vbuf->EndAppendData();
    context->Draw(6 * (UINT)points_to_draw, base_vertex_index);
  }

  stateman->SetGeometryShader(GeometryShaderCache::GetClearGeometryShader());
}

}  // namespace D3D

}  // namespace DX11
