// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D/BoundingBox.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3DCommon/Common.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{
static ComPtr<ID3D11Buffer> s_bbox_buffer;
static ComPtr<ID3D11Buffer> s_bbox_staging_buffer;
static ComPtr<ID3D11UnorderedAccessView> s_bbox_uav;

ID3D11UnorderedAccessView* BBox::GetUAV()
{
  return s_bbox_uav.Get();
}

void BBox::Init()
{
  if (g_ActiveConfig.backend_info.bSupportsBBox)
  {
    // Create 2 buffers here.
    // First for unordered access on default pool.
    auto desc = CD3D11_BUFFER_DESC(4 * sizeof(s32), D3D11_BIND_UNORDERED_ACCESS,
                                   D3D11_USAGE_DEFAULT, 0, 0, 4);
    int initial_values[4] = {0, 0, 0, 0};
    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = initial_values;
    data.SysMemPitch = 4 * sizeof(s32);
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
    UAVdesc.Buffer.NumElements = 4;
    hr = D3D::device->CreateUnorderedAccessView(s_bbox_buffer.Get(), &UAVdesc, &s_bbox_uav);
    CHECK(SUCCEEDED(hr), "Create BoundingBox UAV.");
    D3DCommon::SetDebugObjectName(s_bbox_uav.Get(), "BoundingBox UAV");
    D3D::stateman->SetOMUAV(s_bbox_uav.Get());
  }
}

void BBox::Shutdown()
{
  s_bbox_uav.Reset();
  s_bbox_staging_buffer.Reset();
  s_bbox_buffer.Reset();
}

void BBox::Set(int index, int value)
{
  D3D11_BOX box{index * sizeof(s32), 0, 0, (index + 1) * sizeof(s32), 1, 1};
  D3D::context->UpdateSubresource(s_bbox_buffer.Get(), 0, &box, &value, 0, 0);
}

int BBox::Get(int index)
{
  int data = 0;
  D3D::context->CopyResource(s_bbox_staging_buffer.Get(), s_bbox_buffer.Get());
  D3D11_MAPPED_SUBRESOURCE map;
  HRESULT hr = D3D::context->Map(s_bbox_staging_buffer.Get(), 0, D3D11_MAP_READ, 0, &map);
  if (SUCCEEDED(hr))
  {
    data = ((s32*)map.pData)[index];
  }
  D3D::context->Unmap(s_bbox_staging_buffer.Get(), 0);
  return data;
}
};  // namespace DX11
