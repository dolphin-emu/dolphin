// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D/BoundingBox.h"

#include "VideoCommon/VideoConfig.h"

namespace DX11
{

static ID3D11Buffer* s_bbox_buffer;
static ID3D11Buffer* s_bbox_Readbuffer;
static ID3D11UnorderedAccessView*  s_bbox_uav;

ID3D11UnorderedAccessView* &BBox::GetUAV()
{
	return s_bbox_uav;
}

void BBox::Init()
{
	if (g_ActiveConfig.backend_info.bSupportsBBox)
	{
		// create the pool texture here
		auto desc = CD3D11_BUFFER_DESC(4 * sizeof(s32), D3D11_BIND_UNORDERED_ACCESS, D3D11_USAGE_DEFAULT, 0, 0, 4);
		int initial_values[4] = { 0, 0, 0, 0 };
		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = initial_values;
		data.SysMemPitch = 4 * sizeof(s32);
		data.SysMemSlicePitch = 0;
		HRESULT hr;
		hr = D3D::device->CreateBuffer(&desc, &data, &s_bbox_buffer);
		CHECK(SUCCEEDED(hr), "create bbox buffer");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)s_bbox_buffer, "boundingbox buffer");
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.BindFlags = 0;
		hr = D3D::device->CreateBuffer(&desc, nullptr, &s_bbox_Readbuffer);
		CHECK(SUCCEEDED(hr), "create bbox staging buffer");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)s_bbox_Readbuffer, "boundingbox staging buffer");

		D3D11_UNORDERED_ACCESS_VIEW_DESC UAVdesc;
		memset(&UAVdesc, 0, sizeof(UAVdesc));
		UAVdesc.Format = DXGI_FORMAT_R32_SINT;
		UAVdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		UAVdesc.Buffer.FirstElement = 0;
		UAVdesc.Buffer.Flags = 0;
		UAVdesc.Buffer.NumElements = 4;
		hr = D3D::device->CreateUnorderedAccessView(s_bbox_buffer, &UAVdesc, &s_bbox_uav);
		CHECK(SUCCEEDED(hr), "create bbox UAV");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)s_bbox_uav, "boundingbox UAV");
	}
}

void BBox::Shutdown()
{
	if (s_bbox_buffer != nullptr)
	{
		s_bbox_buffer->Release();
		s_bbox_buffer = nullptr;
	}
	if (s_bbox_Readbuffer != nullptr)
	{
		s_bbox_Readbuffer->Release();
		s_bbox_Readbuffer = nullptr;
	}
	if (s_bbox_uav != nullptr)
	{
		s_bbox_uav->Release();
		s_bbox_uav = nullptr;
	}
}

void BBox::Set(int index, int value)
{
	D3D11_BOX box{ index * sizeof(s32), 0, 0, (index + 1) * sizeof(s32), 1, 1 };
	D3D::context->UpdateSubresource(s_bbox_buffer, 0, &box, &value, 0, 0);
}

int BBox::Get(int index)
{
	int data = 0;
	D3D::context->CopyResource(s_bbox_Readbuffer, s_bbox_buffer);
	D3D11_MAPPED_SUBRESOURCE map;
	HRESULT hr = D3D::context->Map(s_bbox_Readbuffer, 0, D3D11_MAP_READ, 0, &map);
	if (SUCCEEDED(hr))
	{
		data = ((s32*)map.pData)[index];
	}
	D3D::context->Unmap(s_bbox_Readbuffer, 0);
	return data;
}

};
