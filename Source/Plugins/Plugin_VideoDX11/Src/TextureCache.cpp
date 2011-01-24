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

#include "Globals.h"
#include "Statistics.h"
#include "MemoryUtil.h"
#include "Hash.h"

#include "CommonPaths.h"
#include "FileUtil.h"

#include "D3DBase.h"
#include "D3DTexture.h"
#include "D3DUtil.h"
#include "FramebufferManager.h"
#include "PixelShaderCache.h"
#include "PixelShaderManager.h"
#include "VertexShaderManager.h"
#include "VertexShaderCache.h"

#include "Render.h"

#include "TextureDecoder.h"
#include "TextureCache.h"
#include "HiresTextures.h"

namespace DX11
{

#define MAX_COPY_BUFFERS 24
ID3D11Buffer* efbcopycbuf[MAX_COPY_BUFFERS] = {};

TextureCache::TCacheEntry::~TCacheEntry()
{
	texture->Release();
}

void TextureCache::TCacheEntry::Bind(unsigned int stage)
{
	D3D::context->PSSetShaderResources(stage, 1, &texture->GetSRV());
}

bool TextureCache::TCacheEntry::Save(const char filename[])
{
	return SUCCEEDED(PD3DX11SaveTextureToFileA(D3D::context, texture->GetTex(), D3DX11_IFF_PNG, filename));
}

void TextureCache::TCacheEntry::Load(unsigned int width, unsigned int height,
	unsigned int expanded_width, unsigned int level, bool autogen_mips)
{
	D3D::ReplaceRGBATexture2D(texture->GetTex(), TextureCache::temp, width, height, expanded_width, level, usage);

	if (autogen_mips)
		PD3DX11FilterTexture(D3D::context, texture->GetTex(), 0, D3DX11_DEFAULT);
}

TextureCache::TCacheEntryBase* TextureCache::CreateTexture(unsigned int width,
	unsigned int height, unsigned int expanded_width,
	unsigned int tex_levels, PC_TexFormat pcfmt)
{
	D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
	D3D11_CPU_ACCESS_FLAG cpu_access = (D3D11_CPU_ACCESS_FLAG)0;
	D3D11_SUBRESOURCE_DATA srdata, *data = NULL;

	if (tex_levels == 1)
	{
		usage = D3D11_USAGE_DYNAMIC;
		cpu_access = D3D11_CPU_ACCESS_WRITE;

		srdata.pSysMem = TextureCache::temp;
		srdata.SysMemPitch = 4 * expanded_width;

		data = &srdata;
	}

	const D3D11_TEXTURE2D_DESC texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM,
		width, height, 1, tex_levels, D3D11_BIND_SHADER_RESOURCE, usage, cpu_access);

	ID3D11Texture2D *pTexture;
	const HRESULT hr = D3D::device->CreateTexture2D(&texdesc, data, &pTexture);
	CHECK(SUCCEEDED(hr), "Create texture of the TextureCache");

	TCacheEntry* const entry = new TCacheEntry(new D3DTexture2D(pTexture, D3D11_BIND_SHADER_RESOURCE));
	entry->usage = usage;

	// TODO: better debug names
	D3D::SetDebugObjectName((ID3D11DeviceChild*)entry->texture->GetTex(), "a texture of the TextureCache");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)entry->texture->GetSRV(), "shader resource view of a texture of the TextureCache");	

	SAFE_RELEASE(pTexture);

	return entry;
}

void TextureCache::TCacheEntry::FromRenderTarget(bool bFromZBuffer,	bool bScaleByHalf,
	unsigned int cbufid, const float colmat[], const EFBRectangle &source_rect,
	bool bIsIntensityFmt, u32 copyfmt)
{
	g_renderer->ResetAPIState();
	// stretch picture with increased internal resolution
	const D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)virtualW, (float)virtualH);
	D3D::context->RSSetViewports(1, &vp);

	// set transformation
	if (NULL == efbcopycbuf[cbufid])
	{
		const D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(28 * sizeof(float), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DEFAULT);
		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = colmat;
		HRESULT hr = D3D::device->CreateBuffer(&cbdesc, &data, &efbcopycbuf[cbufid]);
		CHECK(SUCCEEDED(hr), "Create efb copy constant buffer %d", cbufid);
		D3D::SetDebugObjectName((ID3D11DeviceChild*)efbcopycbuf[cbufid], "a constant buffer used in TextureCache::CopyRenderTargetToTexture");
	}
	D3D::context->PSSetConstantBuffers(0, 1, &efbcopycbuf[cbufid]);

	const TargetRectangle targetSource = g_renderer->ConvertEFBRectangle(source_rect);
	// TODO: try targetSource.asRECT();
	const D3D11_RECT sourcerect = CD3D11_RECT(targetSource.left, targetSource.top, targetSource.right, targetSource.bottom);

	// Use linear filtering if (bScaleByHalf), use point filtering otherwise
	if (bScaleByHalf)
		D3D::SetLinearCopySampler();
	else
		D3D::SetPointCopySampler();

	D3D::context->OMSetRenderTargets(1, &texture->GetRTV(), NULL);

	D3D::drawShadedTexQuad(
		(bFromZBuffer) ? FramebufferManager::GetEFBDepthTexture()->GetSRV() : FramebufferManager::GetEFBColorTexture()->GetSRV(),
		&sourcerect, Renderer::GetFullTargetWidth(), Renderer::GetFullTargetHeight(),
		(bFromZBuffer) ? PixelShaderCache::GetDepthMatrixProgram(true) : PixelShaderCache::GetColorMatrixProgram(true),
		VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout());

	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());
	
	g_renderer->RestoreAPIState();
}

TextureCache::TCacheEntryBase* TextureCache::CreateRenderTargetTexture(
	unsigned int scaled_tex_w, unsigned int scaled_tex_h)
{
	return new TCacheEntry(D3DTexture2D::Create(scaled_tex_w, scaled_tex_h,
		(D3D11_BIND_FLAG)((int)D3D11_BIND_RENDER_TARGET | (int)D3D11_BIND_SHADER_RESOURCE),
		D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM));
}

TextureCache::TextureCache()
{
}

TextureCache::~TextureCache()
{
	for (unsigned int k = 0; k < MAX_COPY_BUFFERS; ++k)
		SAFE_RELEASE(efbcopycbuf[k]);
}

}
