// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "D3DBase.h"
#include "D3DTexture.h"

#include "png.h"

namespace DX11
{

namespace D3D
{

bool TextureToPng(D3D11_MAPPED_SUBRESOURCE &map, const char* filename, int width, int height, bool saveAlpha)
{
	bool success = false;
	if (map.pData != NULL)
	{
		FILE *fp = NULL;
		png_structp png_ptr = NULL;
		png_infop info_ptr = NULL;

		// Open file for writing (binary mode)
		fp = fopen(filename, "wb");
		if (fp == NULL) {
			PanicAlert("Could not open file %s for writing\n", filename);
			goto finalise;
		}

		// Initialize write structure
		png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		if (png_ptr == NULL) {
			PanicAlert("Could not allocate write struct\n");
			goto finalise;
			
		}

		// Initialize info structure
		info_ptr = png_create_info_struct(png_ptr);
		if (info_ptr == NULL) {
			PanicAlert("Could not allocate info struct\n");
			goto finalise;
		}

		// Setup Exception handling
		if (setjmp(png_jmpbuf(png_ptr))) {
			PanicAlert("Error during png creation\n");
			goto finalise;
		}

		png_init_io(png_ptr, fp);

		// Write header (8 bit colour depth)
		png_set_IHDR(png_ptr, info_ptr, width, height,
			8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

		char title[] = "Dolphin Screenshot";
		png_text title_text;
		title_text.compression = PNG_TEXT_COMPRESSION_NONE;
		title_text.key = "Title";
		title_text.text = title;
		png_set_text(png_ptr, info_ptr, &title_text, 1);

		png_write_info(png_ptr, info_ptr);

		// Write image data
		for (auto y = 0; y < height; ++y)
		{
			u8* row_ptr = (u8*)map.pData + y * map.RowPitch;
			u8* ptr = row_ptr;
			for (UINT x = 0; x < map.RowPitch / 4; ++x)
			{
				if (!saveAlpha)
					ptr[3] = 0xff;
				ptr += 4;
			}
			png_write_row(png_ptr, row_ptr);
		}

		// End write
		png_write_end(png_ptr, NULL);

		success = true;

	finalise:

		if (fp != NULL) fclose(fp);
		if (info_ptr != NULL) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
		if (png_ptr != NULL) png_destroy_write_struct(&png_ptr, (png_infopp)NULL);

	}
	return false;
}

void ReplaceRGBATexture2D(ID3D11Texture2D* pTexture, const u8* buffer, unsigned int width, unsigned int height, unsigned int pitch, unsigned int level, D3D11_USAGE usage)
{
	if (usage == D3D11_USAGE_DYNAMIC || usage == D3D11_USAGE_STAGING)
	{
		D3D11_MAPPED_SUBRESOURCE map;
		D3D::context->Map(pTexture, level, D3D11_MAP_WRITE_DISCARD, 0, &map);
		if (4 * pitch == map.RowPitch)
		{
			memcpy(map.pData, buffer, map.RowPitch * height);
		}
		else
		{
			for (unsigned int y = 0; y < height; ++y)
				memcpy((u8*)map.pData + y * map.RowPitch, (u32*)buffer + y * pitch, 4 * pitch);
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
	if (usage == D3D11_USAGE_STAGING) cpuflags = (D3D11_CPU_ACCESS_FLAG)((int)D3D11_CPU_ACCESS_WRITE|(int)D3D11_CPU_ACCESS_READ);
	else if (usage == D3D11_USAGE_DYNAMIC) cpuflags = D3D11_CPU_ACCESS_WRITE;
	else cpuflags = (D3D11_CPU_ACCESS_FLAG)0;
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

ID3D11Texture2D* &D3DTexture2D::GetTex() { return tex; }
ID3D11ShaderResourceView* &D3DTexture2D::GetSRV() { return srv; }
ID3D11RenderTargetView* &D3DTexture2D::GetRTV() { return rtv; }
ID3D11DepthStencilView* &D3DTexture2D::GetDSV() { return dsv; }

D3DTexture2D::D3DTexture2D(ID3D11Texture2D* texptr, D3D11_BIND_FLAG bind,
							DXGI_FORMAT srv_format, DXGI_FORMAT dsv_format, DXGI_FORMAT rtv_format, bool multisampled)
							: ref(1), tex(texptr), srv(NULL), rtv(NULL), dsv(NULL)
{
	D3D11_SRV_DIMENSION srv_dim = multisampled ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
	D3D11_DSV_DIMENSION dsv_dim = multisampled ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
	D3D11_RTV_DIMENSION rtv_dim = multisampled ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(srv_dim, srv_format);
	D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = CD3D11_DEPTH_STENCIL_VIEW_DESC(dsv_dim, dsv_format);
	D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = CD3D11_RENDER_TARGET_VIEW_DESC(rtv_dim, rtv_format);
	if (bind & D3D11_BIND_SHADER_RESOURCE) D3D::device->CreateShaderResourceView(tex, &srv_desc, &srv);
	if (bind & D3D11_BIND_RENDER_TARGET) D3D::device->CreateRenderTargetView(tex, &rtv_desc, &rtv);
	if (bind & D3D11_BIND_DEPTH_STENCIL) D3D::device->CreateDepthStencilView(tex, &dsv_desc, &dsv);
	tex->AddRef();
}

D3DTexture2D::~D3DTexture2D()
{
	SAFE_RELEASE(srv);
	SAFE_RELEASE(rtv);
	SAFE_RELEASE(dsv);
	SAFE_RELEASE(tex);
}

}  // namespace DX11