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

// Common
#include "CommonPaths.h"
#include "FileUtil.h"
#include "MemoryUtil.h"
#include "Hash.h"

// VideoCommon
#include "VideoConfig.h"
#include "Statistics.h"
#include "PixelShaderManager.h"
#include "VertexShaderManager.h"
#include "TextureDecoder.h"
#include "HiresTextures.h"

// DX11
#include "DX11_D3DBase.h"
#include "DX11_D3DTexture.h"
#include "DX11_D3DUtil.h"
#include "DX11_FramebufferManager.h"
#include "DX11_PixelShaderCache.h"
#include "DX11_VertexShaderCache.h"
#include "DX11_TextureCache.h"

#include "../Main.h"

namespace DX11
{

ID3D11BlendState* efbcopyblendstate = NULL;
ID3D11RasterizerState* efbcopyraststate = NULL;
ID3D11DepthStencilState* efbcopydepthstate = NULL;
ID3D11Buffer* efbcopycbuf[20] = {};

TextureCache::TCacheEntry::~TCacheEntry()
{
	SAFE_RELEASE(texture);
}

void TextureCache::TCacheEntry::Load(unsigned int width, unsigned int height,
	unsigned int expanded_width, unsigned int level)
{
	D3D::ReplaceRGBATexture2D(texture->GetTex(), TextureCache::temp, width, height, expanded_width, level, usage);
}

void TextureCache::TCacheEntry::FromRenderTarget(bool bFromZBuffer,	bool bScaleByHalf,
	unsigned int cbufid, const float colmat[], const EFBRectangle &source_rect)
{
	// stretch picture with increased internal resolution
	const D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)Scaledw, (float)Scaledh);
	D3D::context->RSSetViewports(1, &vp);

	// set transformation
	if (NULL == efbcopycbuf[cbufid])
	{
		const D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(20 * sizeof(float), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DEFAULT);
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

	D3D::stateman->PushBlendState(efbcopyblendstate);
	D3D::stateman->PushRasterizerState(efbcopyraststate);
	D3D::stateman->PushDepthState(efbcopydepthstate);

	D3D::context->OMSetRenderTargets(1, &texture->GetRTV(), NULL);
	
	D3D::drawShadedTexQuad(
		(bFromZBuffer) ? FramebufferManager::GetEFBDepthTexture()->GetSRV() : FramebufferManager::GetEFBColorTexture()->GetSRV(),
		&sourcerect, g_renderer->GetFullTargetWidth(), g_renderer->GetFullTargetHeight(),
		(bFromZBuffer) ? PixelShaderCache::GetDepthMatrixProgram() : PixelShaderCache::GetColorMatrixProgram(),
		VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout());

	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());
	
	D3D::stateman->PopBlendState();
	D3D::stateman->PopDepthState();
	D3D::stateman->PopRasterizerState();
}

void TextureCache::TCacheEntry::Bind(unsigned int stage)
{
	D3D::gfxstate->SetShaderResource(stage, texture->GetSRV());
}

TextureCache::TCacheEntryBase* TextureCache::CreateTexture(unsigned int width,
	unsigned int height, unsigned int expanded_width,
	unsigned int tex_levels, PC_TexFormat pcfmt)
{
	D3D11_SUBRESOURCE_DATA srdata;

	D3D11_SUBRESOURCE_DATA *data = NULL;
	D3D11_CPU_ACCESS_FLAG cpu_access = (D3D11_CPU_ACCESS_FLAG)0;
	D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
	
	// TODO: temp
	tex_levels = 1;

	if (1 == tex_levels)
	{
		cpu_access = D3D11_CPU_ACCESS_WRITE;
		usage = D3D11_USAGE_DYNAMIC;

		srdata.pSysMem = temp;
		srdata.SysMemPitch = 4 * expanded_width;
		data = &srdata;
	}

	const D3D11_TEXTURE2D_DESC texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM,
		width, height, 1, tex_levels, D3D11_BIND_SHADER_RESOURCE, usage, cpu_access);

	ID3D11Texture2D *pTexture;
	HRESULT hr = D3D::device->CreateTexture2D(&texdesc, data, &pTexture);
	CHECK(SUCCEEDED(hr), "Create texture of the TextureCache");

	TCacheEntry* const entry = new TCacheEntry(new D3DTexture2D(pTexture, D3D11_BIND_SHADER_RESOURCE));
	entry->usage = usage;

	// TODO: silly debug names
	D3D::SetDebugObjectName((ID3D11DeviceChild*)entry->texture->GetTex(), "a texture of the TextureCache");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)entry->texture->GetSRV(), "shader resource view of a texture of the TextureCache");	

	// wuts this?
	//if (0 == tex_levels)
	//	PD3DX11FilterTexture(D3D::context, entry->texture->GetTex(), 0, D3DX11_DEFAULT);

	// TODO: this good?
	//if (1 != tex_levels)
	//	entry->Load(width, height, expanded_width, 0);

	SAFE_RELEASE(pTexture);

	return entry;
}

TextureCache::TCacheEntryBase* TextureCache::CreateRenderTargetTexture(int scaled_tex_w, int scaled_tex_h)
{
	return new TCacheEntry(D3DTexture2D::Create(scaled_tex_w, scaled_tex_h,
		(D3D11_BIND_FLAG)((int)D3D11_BIND_RENDER_TARGET | (int)D3D11_BIND_SHADER_RESOURCE),
		D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM));
}

TextureCache::TextureCache()
{
	HRESULT hr;

	D3D11_BLEND_DESC blenddesc;
	blenddesc.AlphaToCoverageEnable = FALSE;
	blenddesc.IndependentBlendEnable = FALSE;
	blenddesc.RenderTarget[0].BlendEnable = FALSE;
	blenddesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	blenddesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blenddesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	blenddesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blenddesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blenddesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blenddesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	hr = D3D::device->CreateBlendState(&blenddesc, &efbcopyblendstate);
	CHECK(hr==S_OK, "Create blend state for CopyRenderTargetToTexture");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)efbcopyblendstate, "blend state used in CopyRenderTargetToTexture");

	D3D11_DEPTH_STENCIL_DESC depthdesc;
	depthdesc.DepthEnable        = FALSE;
	depthdesc.DepthWriteMask     = D3D11_DEPTH_WRITE_MASK_ALL;
	depthdesc.DepthFunc          = D3D11_COMPARISON_LESS;
	depthdesc.StencilEnable      = FALSE;
	depthdesc.StencilReadMask    = D3D11_DEFAULT_STENCIL_READ_MASK;
	depthdesc.StencilWriteMask   = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	hr = D3D::device->CreateDepthStencilState(&depthdesc, &efbcopydepthstate);
	CHECK(hr==S_OK, "Create depth state for CopyRenderTargetToTexture");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)efbcopydepthstate, "depth stencil state used in CopyRenderTargetToTexture");

	D3D11_RASTERIZER_DESC rastdesc;
	rastdesc.CullMode = D3D11_CULL_NONE;
	rastdesc.FillMode = D3D11_FILL_SOLID;
	rastdesc.FrontCounterClockwise = false;
	rastdesc.DepthBias = false;
	rastdesc.DepthBiasClamp = 0;
	rastdesc.SlopeScaledDepthBias = 0;
	rastdesc.DepthClipEnable = false;
	rastdesc.ScissorEnable = false;
	rastdesc.MultisampleEnable = false;
	rastdesc.AntialiasedLineEnable = false;
	hr = D3D::device->CreateRasterizerState(&rastdesc, &efbcopyraststate);
	CHECK(hr==S_OK, "Create rasterizer state for CopyRenderTargetToTexture");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)efbcopyraststate, "rasterizer state used in CopyRenderTargetToTexture");
}

TextureCache::~TextureCache()
{
	SAFE_RELEASE(efbcopyblendstate);
	SAFE_RELEASE(efbcopyraststate);
	SAFE_RELEASE(efbcopydepthstate);

	for (unsigned int k = 0; k < 20; ++k)
		SAFE_RELEASE(efbcopycbuf[k]);
}

}
