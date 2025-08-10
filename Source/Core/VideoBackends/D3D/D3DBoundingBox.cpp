// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3D/D3DBoundingBox.h"

#include <algorithm>
#include <array>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3DCommon/D3DCommon.h"

namespace DX11
{
D3DBoundingBox::~D3DBoundingBox()
{
  m_uav.Reset();
  m_staging_buffer.Reset();
  m_buffer.Reset();
}

bool D3DBoundingBox::Initialize()
{
  // Create 2 buffers here.
  // First for unordered access on default pool.
  auto desc = CD3D11_BUFFER_DESC(NUM_BBOX_VALUES * sizeof(BBoxType), D3D11_BIND_UNORDERED_ACCESS,
                                 D3D11_USAGE_DEFAULT, 0, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS,
                                 sizeof(BBoxType));
  const BBoxType initial_values[NUM_BBOX_VALUES] = {0, 0, 0, 0};
  D3D11_SUBRESOURCE_DATA data;
  data.pSysMem = initial_values;
  data.SysMemPitch = NUM_BBOX_VALUES * sizeof(BBoxType);
  data.SysMemSlicePitch = 0;
  HRESULT hr;
  hr = D3D::device->CreateBuffer(&desc, &data, &m_buffer);
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create BoundingBox Buffer: {}", DX11HRWrap(hr));
  if (FAILED(hr))
    return false;
  D3DCommon::SetDebugObjectName(m_buffer.Get(), "BoundingBox Buffer");

  // Second to use as a staging buffer.
  desc.Usage = D3D11_USAGE_STAGING;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  desc.BindFlags = 0;
  desc.MiscFlags = 0;
  hr = D3D::device->CreateBuffer(&desc, nullptr, &m_staging_buffer);
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create BoundingBox Staging Buffer: {}",
             DX11HRWrap(hr));
  if (FAILED(hr))
    return false;
  D3DCommon::SetDebugObjectName(m_staging_buffer.Get(), "BoundingBox Staging Buffer");

  // UAV is required to allow concurrent access.
  D3D11_UNORDERED_ACCESS_VIEW_DESC UAVdesc = {};
  UAVdesc.Format = DXGI_FORMAT_R32_TYPELESS;
  UAVdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
  UAVdesc.Buffer.FirstElement = 0;
  UAVdesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
  UAVdesc.Buffer.NumElements = NUM_BBOX_VALUES;
  hr = D3D::device->CreateUnorderedAccessView(m_buffer.Get(), &UAVdesc, &m_uav);
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create BoundingBox UAV: {}", DX11HRWrap(hr));
  if (FAILED(hr))
    return false;
  D3DCommon::SetDebugObjectName(m_uav.Get(), "BoundingBox UAV");
  D3D::stateman->SetOMUAV(m_uav.Get());

  return true;
}

std::vector<BBoxType> D3DBoundingBox::Read(u32 index, u32 length)
{
  std::vector<BBoxType> values(length);
  D3D::context->CopyResource(m_staging_buffer.Get(), m_buffer.Get());

  D3D11_MAPPED_SUBRESOURCE map;
  HRESULT hr = D3D::context->Map(m_staging_buffer.Get(), 0, D3D11_MAP_READ, 0, &map);
  if (SUCCEEDED(hr))
  {
    std::memcpy(values.data(), static_cast<const u8*>(map.pData) + sizeof(BBoxType) * index,
                sizeof(BBoxType) * length);

    D3D::context->Unmap(m_staging_buffer.Get(), 0);
  }

  return values;
}

void D3DBoundingBox::Write(u32 index, std::span<const BBoxType> values)
{
  D3D11_BOX box{index * sizeof(BBoxType),
                0,
                0,
                static_cast<u32>((index + values.size()) * sizeof(BBoxType)),
                1,
                1};
  D3D::context->UpdateSubresource(m_buffer.Get(), 0, &box, values.data(), 0, 0);
}

}  // namespace DX11
