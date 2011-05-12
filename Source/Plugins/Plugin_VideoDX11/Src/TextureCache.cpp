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

#include "RenderBase.h"

#include "D3DBase.h"
#include "D3DUtil.h"
#include "FramebufferManager.h"
#include "PixelShaderCache.h"
#include "TextureCache.h"
#include "VertexShaderCache.h"
#include "TextureEncoder.h"
#include "PSTextureEncoder.h"
#include "HW/Memmap.h"
#include "VideoConfig.h"

namespace DX11
{

static std::unique_ptr<TextureEncoder> g_encoder;
const size_t MAX_COPY_BUFFERS = 25;
static SharedPtr<ID3D11Buffer> efbcopycbuf[MAX_COPY_BUFFERS];

void TextureCache::TCacheEntry::Bind(unsigned int stage)
{
	D3D::g_context->PSSetShaderResources(stage, 1, &texture->GetSRV());
}

bool TextureCache::TCacheEntry::Save(const char filename[])
{
	return SUCCEEDED(PD3DX11SaveTextureToFileA(D3D::g_context, texture->GetTex(), D3DX11_IFF_PNG, filename));
}

void TextureCache::TCacheEntry::Load(unsigned int width, unsigned int height,
	unsigned int expanded_width, unsigned int level, bool autogen_mips)
{
	D3D::ReplaceRGBATexture2D(texture->GetTex(), TextureCache::temp, width, height, expanded_width, level, usage);

	if (autogen_mips)
		PD3DX11FilterTexture(D3D::g_context, texture->GetTex(), 0, D3DX11_DEFAULT);
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

	auto const pTexture = CreateTexture2DShared(&texdesc, data);
	CHECK(pTexture, "Create texture of the TextureCache");

	std::unique_ptr<D3DTexture2D> tx(new D3DTexture2D(pTexture, D3D11_BIND_SHADER_RESOURCE));
	auto const entry = new TCacheEntry(std::move(tx));
	entry->usage = usage;

	// TODO: better debug names
	D3D::SetDebugObjectName(entry->texture->GetTex(), "a texture of the TextureCache");
	D3D::SetDebugObjectName(entry->texture->GetSRV(), "shader resource view of a texture of the TextureCache");	

	return entry;
}

void TextureCache::TCacheEntry::FromRenderTarget(u32 dstAddr, unsigned int dstFormat,
	unsigned int srcFormat, const EFBRectangle& srcRect,
	bool isIntensity, bool scaleByHalf, unsigned int cbufid,
	const float *colmat)
{
	if (!isDynamic || g_ActiveConfig.bCopyEFBToTexture)
	{
		g_renderer->ResetAPIState();

		// stretch picture with increased internal resolution
		const D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)virtualW, (float)virtualH);
		D3D::g_context->RSSetViewports(1, &vp);

		// set transformation
		if (NULL == efbcopycbuf[cbufid])
		{
			const D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(28 * sizeof(float), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DEFAULT);
			D3D11_SUBRESOURCE_DATA data;
			data.pSysMem = colmat;
			efbcopycbuf[cbufid] = CreateBufferShared(&cbdesc, &data);
			CHECK(efbcopycbuf[cbufid], "Create efb copy constant buffer %d", cbufid);
			D3D::SetDebugObjectName(efbcopycbuf[cbufid], "a constant buffer used in TextureCache::CopyRenderTargetToTexture");
		}
		D3D::g_context->PSSetConstantBuffers(0, 1, &efbcopycbuf[cbufid]);

		const TargetRectangle targetSource = g_renderer->ConvertEFBRectangle(srcRect);
		// TODO: try targetSource.asRECT();
		const D3D11_RECT sourcerect = CD3D11_RECT(targetSource.left, targetSource.top, targetSource.right, targetSource.bottom);

		// Use linear filtering if (bScaleByHalf), use point filtering otherwise
		if (scaleByHalf)
			D3D::SetLinearCopySampler();
		else
			D3D::SetPointCopySampler();

		D3D::g_context->OMSetRenderTargets(1, &texture->GetRTV(), NULL);

		// Create texture copy
		D3D::drawShadedTexQuad(
			(srcFormat == PIXELFMT_Z24) ? FramebufferManager::GetEFBDepthTexture()->GetSRV() : FramebufferManager::GetEFBColorTexture()->GetSRV(),
			&sourcerect, Renderer::GetTargetWidth(), Renderer::GetTargetHeight(),
			(srcFormat == PIXELFMT_Z24) ? PixelShaderCache::GetDepthMatrixProgram(true) : PixelShaderCache::GetColorMatrixProgram(true),
			VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout());

		D3D::g_context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());
	
		g_renderer->RestoreAPIState();
	}

	if (!g_ActiveConfig.bCopyEFBToTexture)
	{
		u8* dst = Memory::GetPointer(dstAddr);
		size_t encodeSize = g_encoder->Encode(dst, dstFormat, srcFormat, srcRect, isIntensity, scaleByHalf);
		hash = GetHash64(dst, (int)encodeSize, g_ActiveConfig.iSafeTextureCache_ColorSamples);
		if (g_ActiveConfig.bEFBCopyCacheEnable)
		{
			// If the texture in RAM is already in the texture cache,
			// do not copy it again as it has not changed.
			if (TextureCache::Find(dstAddr, hash))
				return;
		}

		TextureCache::MakeRangeDynamic(dstAddr, (u32)encodeSize);
	}
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
	// FIXME: Is it safe here?
	g_encoder.reset(new PSTextureEncoder);
}

TextureCache::~TextureCache()
{
	g_encoder.reset();
}

}
