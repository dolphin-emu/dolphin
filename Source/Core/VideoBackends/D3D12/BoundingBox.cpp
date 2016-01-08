// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D12/BoundingBox.h"

#include "VideoCommon/VideoConfig.h"

// D3D12TODO: Support bounding box behavior.
namespace DX12
{

static ID3D11Buffer* s_bbox_buffer;
static ID3D11Buffer* s_bbox_staging_buffer;
static ID3D11UnorderedAccessView*  s_bbox_uav;

ID3D11UnorderedAccessView* &BBox::GetUAV()
{
	return s_bbox_uav;
}

void BBox::Init()
{
	if (g_ActiveConfig.backend_info.bSupportsBBox)
	{
#ifdef USE_D3D11
		// Create 2 buffers here.
		// First for unordered access on default pool.
		auto desc = CD3D11_BUFFER_DESC(4 * sizeof(s32), D3D11_BIND_UNORDERED_ACCESS, D3D11_USAGE_DEFAULT, 0, 0, 4);
		int initial_values[4] = { 0, 0, 0, 0 };
		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = initial_values;
		data.SysMemPitch = 4 * sizeof(s32);
		data.SysMemSlicePitch = 0;
		HRESULT hr;
		hr = D3D::device->CreateBuffer(&desc, &data, &s_bbox_buffer);
		CHECK(SUCCEEDED(hr), "Create BoundingBox Buffer.");
		D3D::SetDebugObjectName(s_bbox_buffer, "BoundingBox Buffer");

		// Second to use as a staging buffer.
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.BindFlags = 0;
		hr = D3D::device->CreateBuffer(&desc, nullptr, &s_bbox_staging_buffer);
		CHECK(SUCCEEDED(hr), "Create BoundingBox Staging Buffer.");
		D3D::SetDebugObjectName(s_bbox_staging_buffer, "BoundingBox Staging Buffer");

		// UAV is required to allow concurrent access.
		D3D11_UNORDERED_ACCESS_VIEW_DESC UAVdesc = {};
		UAVdesc.Format = DXGI_FORMAT_R32_SINT;
		UAVdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		UAVdesc.Buffer.FirstElement = 0;
		UAVdesc.Buffer.Flags = 0;
		UAVdesc.Buffer.NumElements = 4;
		hr = D3D::device->CreateUnorderedAccessView(s_bbox_buffer, &UAVdesc, &s_bbox_uav);
		CHECK(SUCCEEDED(hr), "Create BoundingBox UAV.");
		D3D::SetDebugObjectName(s_bbox_uav, "BoundingBox UAV");
#endif
	}
}

void BBox::Shutdown()
{
	SAFE_RELEASE(s_bbox_buffer);
	SAFE_RELEASE(s_bbox_staging_buffer);
	SAFE_RELEASE(s_bbox_uav);
}

void BBox::Set(int index, int value)
{
	D3D11_BOX box{ index * sizeof(s32), 0, 0, (index + 1) * sizeof(s32), 1, 1 };
#ifdef USE_D3D11
	D3D::context->UpdateSubresource(s_bbox_buffer, 0, &box, &value, 0, 0);
#endif
}

int BBox::Get(int index)
{
#ifdef USE_D3D11
	int data = 0;
	D3D::context->CopyResource(s_bbox_staging_buffer, s_bbox_buffer);
	D3D11_MAPPED_SUBRESOURCE map;
	HRESULT hr = D3D::context->Map(s_bbox_staging_buffer, 0, D3D11_MAP_READ, 0, &map);
	if (SUCCEEDED(hr))
	{
		data = static_cast<s32*>(map.pData)[index];
	}
	D3D::context->Unmap(s_bbox_staging_buffer, 0);
	return data;
#else
	return 0;
#endif
}

};
