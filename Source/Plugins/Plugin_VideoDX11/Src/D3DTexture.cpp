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

#include "D3DBase.h"
#include "D3DTexture.h"

namespace DX11
{

namespace D3D
{

void ReplaceRGBATexture2D(ID3D11Texture2D* pTexture, const u8* buffer, unsigned int width, unsigned int height, unsigned int pitch, unsigned int level, D3D11_USAGE usage)
{
	if (usage == D3D11_USAGE_DYNAMIC || usage == D3D11_USAGE_STAGING)
	{
		D3D11_MAPPED_SUBRESOURCE map;
		D3D::g_context->Map(pTexture, level, D3D11_MAP_WRITE_DISCARD, 0, &map);
		if (4 * pitch == map.RowPitch)
		{
			memcpy(map.pData, buffer, map.RowPitch * height);
		}
		else
		{
			for (unsigned int y = 0; y < height; ++y)
				memcpy((u8*)map.pData + y * map.RowPitch, (u32*)buffer + y * pitch, 4 * pitch);
		}
		D3D::g_context->Unmap(pTexture, level);
	}
	else
	{
		D3D11_BOX dest_region = CD3D11_BOX(0, 0, 0, width, height, 1);
		D3D::g_context->UpdateSubresource(pTexture, level, &dest_region, buffer, 4*pitch, 4*pitch*height);
	}
}

}  // namespace

std::unique_ptr<D3DTexture2D> D3DTexture2D::Create(unsigned int width, unsigned int height, D3D11_BIND_FLAG bind,
	D3D11_USAGE usage, DXGI_FORMAT fmt, unsigned int levels)
{
	D3D11_CPU_ACCESS_FLAG cpuflags;
	if (usage == D3D11_USAGE_STAGING)
		cpuflags = (D3D11_CPU_ACCESS_FLAG)(D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ);
	else if (usage == D3D11_USAGE_DYNAMIC)
		cpuflags = D3D11_CPU_ACCESS_WRITE;
	else
		cpuflags = (D3D11_CPU_ACCESS_FLAG)0;
	
	const D3D11_TEXTURE2D_DESC texdesc = CD3D11_TEXTURE2D_DESC(fmt, width, height, 1, levels, bind, usage, cpuflags);
	
	auto texture = CreateTexture2DShared(&texdesc, NULL);
	if (!texture)
	{
		PanicAlert("Failed to create texture at %s, line %d\n", __FILE__, __LINE__);
		return NULL;
	}

	return std::unique_ptr<D3DTexture2D>(new D3DTexture2D(texture, bind));
}

D3DTexture2D::D3DTexture2D(SharedPtr<ID3D11Texture2D> texptr, D3D11_BIND_FLAG bind,
	DXGI_FORMAT srv_format, DXGI_FORMAT dsv_format, DXGI_FORMAT rtv_format, bool multisampled)
	: ref(1), tex(texptr), srv(NULL), rtv(NULL), dsv(NULL)
{
	D3D11_SRV_DIMENSION srv_dim = multisampled ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
	D3D11_DSV_DIMENSION dsv_dim = multisampled ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
	D3D11_RTV_DIMENSION rtv_dim = multisampled ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(srv_dim, srv_format);
	D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = CD3D11_DEPTH_STENCIL_VIEW_DESC(dsv_dim, dsv_format);
	D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = CD3D11_RENDER_TARGET_VIEW_DESC(rtv_dim, rtv_format);
	if (bind & D3D11_BIND_SHADER_RESOURCE)
		D3D::g_device->CreateShaderResourceView(tex, &srv_desc, &srv);
	if (bind & D3D11_BIND_RENDER_TARGET)
		D3D::g_device->CreateRenderTargetView(tex, &rtv_desc, &rtv);
	if (bind & D3D11_BIND_DEPTH_STENCIL)
		D3D::g_device->CreateDepthStencilView(tex, &dsv_desc, &dsv);
}

D3DTexture2D::~D3DTexture2D()
{
	SAFE_RELEASE(srv);
	SAFE_RELEASE(rtv);
	SAFE_RELEASE(dsv);
}

}  // namespace DX11