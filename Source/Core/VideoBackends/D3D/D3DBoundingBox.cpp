// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "VideoBackends/D3D/D3DBoundingBox.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3DCommon/D3DCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{
static constexpr u32 NUM_BBOX_VALUES = 4;
static ComPtr<ID3D11Buffer> s_bbox_buffer;
static ComPtr<ID3D11Buffer> s_bbox_staging_buffer;
static ComPtr<ID3D11UnorderedAccessView> s_bbox_uav;
static std::array<s32, NUM_BBOX_VALUES> s_bbox_values;
static std::array<bool, NUM_BBOX_VALUES> s_bbox_dirty;
static bool s_bbox_valid = false;

ID3D11UnorderedAccessView* BBox::GetUAV()
{
  return s_bbox_uav.Get();
}

void BBox::Init()
{
  if (!g_ActiveConfig.backend_info.bSupportsBBox)
    return;

  // Create 2 buffers here.
  // First for unordered access on default pool.
  auto desc = CD3D11_BUFFER_DESC(NUM_BBOX_VALUES * sizeof(s32), D3D11_BIND_UNORDERED_ACCESS,
                                 D3D11_USAGE_DEFAULT, 0, 0, sizeof(s32));
  const s32 initial_values[NUM_BBOX_VALUES] = {0, 0, 0, 0};
  D3D11_SUBRESOURCE_DATA data;
  data.pSysMem = initial_values;
  data.SysMemPitch = NUM_BBOX_VALUES * sizeof(s32);
  data.SysMemSlicePitch = 0;
  HRESULT hr;
  hr = D3D::device->CreateBuffer(&desc, &data, &s_bbox_buffer);
  CHECK(SUCCEEDED(hr), "Create BoundingBox Buffer.");
  D3DCommon::SetDebugObjectName(s_bbox_buffer.Get(), "BoundingBox Buffer");

  // Second to use as a staging buffer.
  desc.Usage = D3D11_USAGE_STAGING;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  desc.BindFlags = 0;
  hr = D3D::device->CreateBuffer(&desc, nullptr, &s_bbox_staging_buffer);
  CHECK(SUCCEEDED(hr), "Create BoundingBox Staging Buffer.");
  D3DCommon::SetDebugObjectName(s_bbox_staging_buffer.Get(), "BoundingBox Staging Buffer");

  // UAV is required to allow concurrent access.
  D3D11_UNORDERED_ACCESS_VIEW_DESC UAVdesc = {};
  UAVdesc.Format = DXGI_FORMAT_R32_SINT;
  UAVdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
  UAVdesc.Buffer.FirstElement = 0;
  UAVdesc.Buffer.Flags = 0;
  UAVdesc.Buffer.NumElements = NUM_BBOX_VALUES;
  hr = D3D::device->CreateUnorderedAccessView(s_bbox_buffer.Get(), &UAVdesc, &s_bbox_uav);
  CHECK(SUCCEEDED(hr), "Create BoundingBox UAV.");
  D3DCommon::SetDebugObjectName(s_bbox_uav.Get(), "BoundingBox UAV");
  D3D::stateman->SetOMUAV(s_bbox_uav.Get());

  s_bbox_dirty = {};
  s_bbox_valid = true;
}

void BBox::Shutdown()
{
  s_bbox_uav.Reset();
  s_bbox_staging_buffer.Reset();
  s_bbox_buffer.Reset();
}

void BBox::Flush()
{
  s_bbox_valid = false;

  if (std::none_of(s_bbox_dirty.begin(), s_bbox_dirty.end(), [](bool dirty) { return dirty; }))
    return;

  for (u32 start = 0; start < NUM_BBOX_VALUES;)
  {
    if (!s_bbox_dirty[start])
    {
      start++;
      continue;
    }

    u32 end = start + 1;
    s_bbox_dirty[start] = false;
    for (; end < NUM_BBOX_VALUES; end++)
    {
      if (!s_bbox_dirty[end])
        break;

      s_bbox_dirty[end] = false;
    }

    D3D11_BOX box{start * sizeof(s32), 0, 0, end * sizeof(s32), 1, 1};
    D3D::context->UpdateSubresource(s_bbox_buffer.Get(), 0, &box, &s_bbox_values[start], 0, 0);
  }
}

void BBox::Readback()
{
  D3D::context->CopyResource(s_bbox_staging_buffer.Get(), s_bbox_buffer.Get());

  D3D11_MAPPED_SUBRESOURCE map;
  HRESULT hr = D3D::context->Map(s_bbox_staging_buffer.Get(), 0, D3D11_MAP_READ, 0, &map);
  if (SUCCEEDED(hr))
  {
    for (u32 i = 0; i < NUM_BBOX_VALUES; i++)
    {
      if (!s_bbox_dirty[i])
      {
        std::memcpy(&s_bbox_values[i], reinterpret_cast<const u8*>(map.pData) + sizeof(s32) * i,
                    sizeof(s32));
      }
    }

    D3D::context->Unmap(s_bbox_staging_buffer.Get(), 0);
  }

  s_bbox_valid = true;
}

void BBox::Set(int index, int value)
{
  if (s_bbox_valid && s_bbox_values[index] == value)
    return;

  s_bbox_values[index] = value;
  s_bbox_dirty[index] = true;
}

int BBox::Get(int index)
{
  if (!s_bbox_valid)
    Readback();

  return s_bbox_values[index];
}
};  // namespace DX11
