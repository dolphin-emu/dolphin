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

#include "d3dx11.h"
#include "D3DBase.h"
#include "D3DTexture.h"

namespace D3D
{

// buffers for storing the data for DEFAULT textures
const char* texbuf = NULL;
unsigned int texbufsize = 0;

void ReplaceTexture2D(ID3D11Texture2D* pTexture, const u8* buffer, unsigned int width, unsigned int height, unsigned int pitch, DXGI_FORMAT fmt, PC_TexFormat pcfmt, unsigned int level, D3D11_USAGE usage)
{
	void* outptr;
	unsigned int destPitch;
	bool bExpand = false;

	if (usage == D3D11_USAGE_DYNAMIC || usage == D3D11_USAGE_STAGING)
	{
		if (level != 0) PanicAlert("Dynamic textures don't support mipmaps, but given level is not 0 at %s %d\n", __FILE__, __LINE__);
		D3D11_MAPPED_SUBRESOURCE map;
		D3D::context->Map(pTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		outptr = map.pData;
		destPitch = map.RowPitch;
	}
	else if (usage == D3D11_USAGE_DEFAULT)
	{
		if (texbufsize < 4*width*height)
		{
			if (texbuf) delete[] texbuf;
			texbuf = new char[4*width*height];
			texbufsize = 4*width*height;
		}
		outptr = (void*)texbuf;
		destPitch = width * 4;
	}
	else
	{
		PanicAlert("ReplaceTexture2D called on an immutable texture!\n");
		return;
	}

	// TODO: Merge the conversions done here to VideoDecoder
	switch (pcfmt)
	{
		case PC_TEX_FMT_IA4_AS_IA8:
		case PC_TEX_FMT_IA8:
			for (unsigned int y = 0; y < height; y++)
			{
				u16* in = (u16*)buffer + y * pitch;
				u32* pBits = (u32*)((u8*)outptr + y * destPitch);
				for (unsigned int x = 0; x < width; x++)
				{
					const u8 I = (*in & 0xFF);
					const u8 A = (*in & 0xFF00) >> 8;
					*pBits = (A << 24) | (I << 16) | (I << 8) | I;
					in++;
					pBits++;
				}
			}
			break;
		case PC_TEX_FMT_I8:
		case PC_TEX_FMT_I4_AS_I8:
			{
				const u8 *pIn = buffer;
				for (int y = 0; y < height; y++)
				{
					u8* pBits = ((u8*)outptr + (y * destPitch));
					for(int i = 0; i < width * 4; i += 4) 
					{
						pBits[i] = pIn[i / 4];
						pBits[i+1] = pIn[i / 4];
						pBits[i+2] = pIn[i / 4];
						pBits[i + 3] = pIn[i / 4];
					}
					pIn += pitch;
				}
			}
			
			break;
		case PC_TEX_FMT_BGRA32:
			// BGRA32 textures can be uploaded directly to VRAM when using DEFAULT textures
			if (usage == D3D11_USAGE_DEFAULT) break;
			for (unsigned int y = 0; y < height; y++)
			{
				u32* in = (u32*)buffer + y * pitch;
				u32* pBits = (u32*)((u8*)outptr + y * destPitch);
				for (unsigned int x = 0; x < width; x++)
				{
					const u32 col = *in;
					*pBits = col;
					in++;
					pBits++;
				}
			}
			break;
		case PC_TEX_FMT_RGB565:
			for (unsigned int y = 0; y < height; y++)
			{
				u16* in = (u16*)buffer + y * pitch;
				u32* pBits = (u32*)((u8*)outptr + y * destPitch);
				for (unsigned int x = 0; x < width; x++)
				{
					const u16 col = *in;
					*pBits = 0xFF000000 | (((col&0x1f)<<3)) | ((col&0x7e0)<<5) | ((col&0xF800)<<8);
					pBits++;
					in++;
				}
			}
			break;
		default:
			PanicAlert("Unknown tex fmt %d\n", pcfmt);
			break;
	}
	if (usage == D3D11_USAGE_DYNAMIC)
	{
		// TODO: UpdateSubresource might be faster than mapping
		D3D::context->Unmap(pTexture, 0);
	}
	else if (usage == D3D11_USAGE_DEFAULT)
	{
		D3D11_BOX dest_region = CD3D11_BOX(0, 0, 0, width, height, 1);
		if (pcfmt == PC_TEX_FMT_BGRA32)
			D3D::context->UpdateSubresource(pTexture, level, &dest_region, buffer, 4*pitch, 4*(4*pitch)*height);
		else
			D3D::context->UpdateSubresource(pTexture, level, &dest_region, outptr, destPitch, 4*width*height);
	}
}

}  // namespace

D3DTexture2D* D3DTexture2D::Create(unsigned int width, unsigned int height, D3D11_BIND_FLAG bind, D3D11_USAGE usage, DXGI_FORMAT fmt, unsigned int levels)
{
	ID3D11Texture2D* pTexture = NULL;
	HRESULT hr;

	D3D11_CPU_ACCESS_FLAG cpuflags;
	if (usage == D3D11_USAGE_STAGING) cpuflags = (D3D11_CPU_ACCESS_FLAG)((int)D3D11_CPU_ACCESS_WRITE|(int)D3D11_CPU_ACCESS_READ);
	else if (usage == D3D11_USAGE_DYNAMIC) cpuflags = D3D11_CPU_ACCESS_WRITE;
	else cpuflags = (D3D11_CPU_ACCESS_FLAG)0;
	D3D11_TEXTURE2D_DESC texdesc = CD3D11_TEXTURE2D_DESC(fmt, width, height, 1, levels, bind, usage, cpuflags);
	hr = D3D::device->CreateTexture2D(&texdesc, NULL, &pTexture);
	if (FAILED(hr))
	{
		PanicAlert("Failed to create texture at %s %d\n", __FILE__, __LINE__);
		return NULL;
	}

	D3DTexture2D* ret = new D3DTexture2D(pTexture, bind);
	pTexture->Release();
	return ret;
}

void D3DTexture2D::AddRef()
{
	ref++;
}

UINT D3DTexture2D::Release()
{
	ref--;
	if (ref == 0)
	{
		delete this;
		return 0;
	}
	return ref;
}

ID3D11Texture2D* &D3DTexture2D::GetTex() { return tex; }
ID3D11ShaderResourceView* &D3DTexture2D::GetSRV() { return srv; }
ID3D11RenderTargetView* &D3DTexture2D::GetRTV() { return rtv; }
ID3D11DepthStencilView* &D3DTexture2D::GetDSV() { return dsv; }

D3DTexture2D::D3DTexture2D(ID3D11Texture2D* texptr, D3D11_BIND_FLAG bind,
							DXGI_FORMAT srv_format, DXGI_FORMAT dsv_format, DXGI_FORMAT rtv_format)
							: ref(1), tex(texptr), srv(NULL), rtv(NULL), dsv(NULL)
{
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, srv_format);
	D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = CD3D11_DEPTH_STENCIL_VIEW_DESC(D3D11_DSV_DIMENSION_TEXTURE2D, dsv_format);
	D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = CD3D11_RENDER_TARGET_VIEW_DESC(D3D11_RTV_DIMENSION_TEXTURE2D, rtv_format);
	if (bind & D3D11_BIND_SHADER_RESOURCE) D3D::device->CreateShaderResourceView(tex, &srv_desc, &srv);
	if (bind & D3D11_BIND_RENDER_TARGET) D3D::device->CreateRenderTargetView(tex, &rtv_desc, &rtv);
	if (bind & D3D11_BIND_DEPTH_STENCIL) D3D::device->CreateDepthStencilView(tex, &dsv_desc, &dsv);
	tex->AddRef();
}

D3DTexture2D::~D3DTexture2D()
{
	if (srv) srv->Release();
	if (rtv) rtv->Release();
	if (dsv) dsv->Release();
	tex->Release();
}
