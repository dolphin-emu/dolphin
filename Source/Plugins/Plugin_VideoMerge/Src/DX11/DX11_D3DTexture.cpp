// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <d3dx11.h>
#include "DX11_D3DBase.h"
#include "DX11_D3DTexture.h"

namespace DX11
{

namespace D3D
{

void ReplaceRGBATexture2D(ID3D11Texture2D* pTexture, const u8* buffer, unsigned int width, unsigned int height, unsigned int pitch, unsigned int level, D3D11_USAGE usage)
{
	if (usage == D3D11_USAGE_DYNAMIC)
	{
		D3D11_MAPPED_SUBRESOURCE map;
		HRESULT hr = D3D::context->Map(pTexture, level, D3D11_MAP_WRITE_DISCARD, 0, &map);
		CHECK(SUCCEEDED(hr), "ID3D11DeviceContext::Map failed! (%x)", hr);
		if (4 * pitch == map.RowPitch)
		{
			memcpy(map.pData, buffer, map.RowPitch * height);
		}
		else
		{
			for (unsigned int y = 0; y < height; ++y)
				memcpy((u8*)map.pData + y * map.RowPitch, (u32*)buffer + y * pitch, map.RowPitch);
		}
		D3D::context->Unmap(pTexture, level);
	}
	else
	{
		D3D11_BOX dest_region = CD3D11_BOX(0, 0, 0, width, height, 1);
		D3D::context->UpdateSubresource(pTexture, level, &dest_region, buffer, 4*pitch, 4*pitch*height);
	}
}

}  // namespace

D3DTexture2D* D3DTexture2D::Create(unsigned int width, unsigned int height, D3D11_BIND_FLAG bind, D3D11_USAGE usage, DXGI_FORMAT fmt, unsigned int levels)
{
	ID3D11Texture2D* pTexture = NULL;
	HRESULT hr;

	D3D11_CPU_ACCESS_FLAG cpuflags;
	if (usage == D3D11_USAGE_STAGING)
		cpuflags = (D3D11_CPU_ACCESS_FLAG)((int)D3D11_CPU_ACCESS_WRITE|(int)D3D11_CPU_ACCESS_READ);
	else if (usage == D3D11_USAGE_DYNAMIC)
		cpuflags = D3D11_CPU_ACCESS_WRITE;
	else
		cpuflags = (D3D11_CPU_ACCESS_FLAG)0;

	D3D11_TEXTURE2D_DESC texdesc = CD3D11_TEXTURE2D_DESC(fmt, width, height, 1, levels, bind, usage, cpuflags);
	hr = D3D::device->CreateTexture2D(&texdesc, NULL, &pTexture);
	if (FAILED(hr))
	{
		PanicAlert("Failed to create texture at %s, line %d: hr=%#x\n", __FILE__, __LINE__, hr);
		return NULL;
	}

	D3DTexture2D* ret = new D3DTexture2D(pTexture, bind);
	SAFE_RELEASE(pTexture);
	return ret;
}

void D3DTexture2D::AddRef()
{
	++ref;
}

UINT D3DTexture2D::Release()
{
	--ref;
	if (ref == 0)
	{
		delete this;
		return 0;
	}
	return ref;
}

D3DTexture2D::D3DTexture2D(ID3D11Texture2D* texptr, D3D11_BIND_FLAG bind,
							DXGI_FORMAT srv_format, DXGI_FORMAT dsv_format, DXGI_FORMAT rtv_format)
							: ref(1), tex(texptr), srv(NULL), rtv(NULL), dsv(NULL)
{
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, srv_format);
	D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = CD3D11_DEPTH_STENCIL_VIEW_DESC(D3D11_DSV_DIMENSION_TEXTURE2D, dsv_format);
	D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = CD3D11_RENDER_TARGET_VIEW_DESC(D3D11_RTV_DIMENSION_TEXTURE2D, rtv_format);
	
	if (bind & D3D11_BIND_SHADER_RESOURCE)
		D3D::device->CreateShaderResourceView(tex, &srv_desc, &srv);

	if (bind & D3D11_BIND_RENDER_TARGET)
		D3D::device->CreateRenderTargetView(tex, &rtv_desc, &rtv);

	if (bind & D3D11_BIND_DEPTH_STENCIL)
		D3D::device->CreateDepthStencilView(tex, &dsv_desc, &dsv);

	tex->AddRef();
}

D3DTexture2D::~D3DTexture2D()
{
	SAFE_RELEASE(srv);
	SAFE_RELEASE(rtv);
	SAFE_RELEASE(dsv);
	SAFE_RELEASE(tex);
}

}
