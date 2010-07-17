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
#include "FBManager.h"
#include "PixelShaderCache.h"
#include "PixelShaderManager.h"
#include "VertexShaderManager.h"
#include "VertexShaderCache.h"

#include "Render.h"

#include "TextureDecoder.h"
#include "TextureCache.h"
#include "HiresTextures.h"

ID3D11BlendState* efbcopyblendstate = NULL;
ID3D11RasterizerState* efbcopyraststate = NULL;
ID3D11DepthStencilState* efbcopydepthstate = NULL;
ID3D11Buffer* efbcopycbuf[20] = { NULL };

u8* TextureCache::temp = NULL;
TextureCache::TexCache TextureCache::textures;

extern int frameCount;

#define TEMP_SIZE (1024*1024*4)
#define TEXTURE_KILL_THRESHOLD 200

void TextureCache::TCacheEntry::Destroy(bool shutdown)
{
	SAFE_RELEASE(texture);

	if (!isRenderTarget && !shutdown && !g_ActiveConfig.bSafeTextureCache)
	{
		u32* ptr = (u32*)g_VideoInitialize.pGetMemoryPointer(addr);
		if (ptr && *ptr == hash)
			*ptr = oldpixel;
	}
}

void TextureCache::Init()
{
	HRESULT hr;

	temp = (u8*)AllocateMemoryPages(TEMP_SIZE);
	TexDecoder_SetTexFmtOverlayOptions(g_ActiveConfig.bTexFmtOverlayEnable, g_ActiveConfig.bTexFmtOverlayCenter);
	HiresTextures::Init(globals->unique_id);

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
	CHECK(hr==S_OK, "Create blend state for TextureCache::CopyRenderTargetToTexture");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)efbcopyblendstate, "blend state used in TextureCache::CopyRenderTargetToTexture");

	D3D11_DEPTH_STENCIL_DESC depthdesc;
	depthdesc.DepthEnable        = FALSE;
	depthdesc.DepthWriteMask     = D3D11_DEPTH_WRITE_MASK_ALL;
	depthdesc.DepthFunc          = D3D11_COMPARISON_LESS;
	depthdesc.StencilEnable      = FALSE;
	depthdesc.StencilReadMask    = D3D11_DEFAULT_STENCIL_READ_MASK;
	depthdesc.StencilWriteMask   = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	hr = D3D::device->CreateDepthStencilState(&depthdesc, &efbcopydepthstate);
	CHECK(hr==S_OK, "Create depth state for TextureCache::CopyRenderTargetToTexture");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)efbcopydepthstate, "depth stencil state used in TextureCache::CopyRenderTargetToTexture");

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
	CHECK(hr==S_OK, "Create rasterizer state for TextureCache::CopyRenderTargetToTexture");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)efbcopyraststate, "rasterizer state used in TextureCache::CopyRenderTargetToTexture");
}

void TextureCache::Invalidate(bool shutdown)
{
	for (TexCache::iterator iter = textures.begin(); iter != textures.end(); ++iter)
		iter->second.Destroy(shutdown);
	textures.clear();
	HiresTextures::Shutdown();
}

void TextureCache::InvalidateRange(u32 start_address, u32 size)
{
	TexCache::iterator iter = textures.begin();
	while (iter != textures.end())
	{
		if (iter->second.IntersectsMemoryRange(start_address, size))
		{
			iter->second.Destroy(false);
			textures.erase(iter++);
		}
		else
		{
			++iter;
		}
	}
}

bool TextureCache::TCacheEntry::IntersectsMemoryRange(u32 range_address, u32 range_size)
{
	if (addr + size_in_bytes < range_address) return false;
	if (addr >= range_address + range_size) return false;
	return true;
}

void TextureCache::Shutdown()
{
	Invalidate(true);
	FreeMemoryPages(temp, TEMP_SIZE);
	temp = NULL;

	SAFE_RELEASE(efbcopyblendstate);
	SAFE_RELEASE(efbcopyraststate);
	SAFE_RELEASE(efbcopydepthstate);

	for (unsigned int k = 0; k < 20;k++)
		SAFE_RELEASE(efbcopycbuf[k]);
}

void TextureCache::Cleanup()
{
	TexCache::iterator iter = textures.begin();
	while (iter != textures.end())
	{
		if (frameCount > TEXTURE_KILL_THRESHOLD + iter->second.frameCount)
		{
			iter->second.Destroy(false);
			iter = textures.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}

// returns the exponent of the smallest power of two which is greater than val
unsigned int GetPow2(unsigned int val)
{
	unsigned int ret = 0;
	for (;val;val>>=1) ret++;
	return ret;
}

TextureCache::TCacheEntry* TextureCache::Load(unsigned int stage, u32 address, unsigned int width, unsigned int height, unsigned int tex_format, unsigned int tlutaddr, unsigned int tlutfmt, bool UseNativeMips, unsigned int maxlevel)
{
	if (address == 0) return NULL;

	u8* ptr = g_VideoInitialize.pGetMemoryPointer(address);
	unsigned int bsw = TexDecoder_GetBlockWidthInTexels(tex_format) - 1; // TexelSizeInNibbles(format)*width*height/16;
	unsigned int bsh = TexDecoder_GetBlockHeightInTexels(tex_format) - 1; // TexelSizeInNibbles(format)*width*height/16;
	unsigned int bsdepth = TexDecoder_GetTexelSizeInNibbles(tex_format);
	unsigned int expandedWidth  = (width  + bsw) & (~bsw);
	unsigned int expandedHeight = (height + bsh) & (~bsh);

	u64 hash_value;
	u32 texID = address;
	u64 texHash;
	u32 FullFormat = tex_format;
	if ((tex_format == GX_TF_C4) || (tex_format == GX_TF_C8) || (tex_format == GX_TF_C14X2))
		u32 FullFormat = (tex_format | (tlutfmt << 16));

	// hires textures and texture dumping not supported, yet
	if (g_ActiveConfig.bSafeTextureCache/* || g_ActiveConfig.bHiresTextures || g_ActiveConfig.bDumpTextures*/)
	{
		texHash = TexDecoder_GetHash64(ptr,TexDecoder_GetTextureSizeInBytes(expandedWidth, expandedHeight, tex_format),g_ActiveConfig.iSafeTextureCache_ColorSamples);
		if ((tex_format == GX_TF_C4) || (tex_format == GX_TF_C8) || (tex_format == GX_TF_C14X2))
		{
			// WARNING! texID != address now => may break CopyRenderTargetToTexture (cf. TODO up)
			// tlut size can be up to 32768B (GX_TF_C14X2) but Safer == Slower.
			// This trick (to change the texID depending on the TLUT addr) is a trick to get around
			// an issue with metroid prime's fonts, where it has multiple sets of fonts on top of
			// each other stored in a single texture, and uses the palette to make different characters
			// visible or invisible. Thus, unless we want to recreate the textures for every drawn character,
			// we must make sure that texture with different tluts get different IDs.
			u64 tlutHash = TexDecoder_GetHash64(&texMem[tlutaddr], TexDecoder_GetPaletteSize(tex_format),g_ActiveConfig.iSafeTextureCache_ColorSamples);
			texHash ^= tlutHash;
			if (g_ActiveConfig.bSafeTextureCache)
			{
				texID = texID ^ ((u32)(tlutHash & 0xFFFFFFFF)) ^ ((u32)((tlutHash >> 32) & 0xFFFFFFFF));
			}
		}
		if (g_ActiveConfig.bSafeTextureCache)
			hash_value = texHash;
	}

	bool skip_texture_create = false;
	TexCache::iterator iter = textures.find(texID);

	if (iter != textures.end())
	{
		TCacheEntry &entry = iter->second;

		if (!g_ActiveConfig.bSafeTextureCache)
			hash_value = ((u32*)ptr)[0];

		// TODO: Is the (entry.MipLevels == maxlevel) check needed?
		if (entry.isRenderTarget || ((address == entry.addr) && (hash_value == entry.hash) && FullFormat == entry.fmt && entry.MipLevels == maxlevel))
		{
			entry.frameCount = frameCount;
			D3D::gfxstate->SetShaderResource(stage, entry.texture->GetSRV());
			return &entry;
		}
		else
		{
			// Let's reload the new texture data into the same texture,
			// instead of destroying it and having to create a new one.
			// Might speed up movie playback very, very slightly.

			// TODO: Is the (entry.MipLevels < maxlevel) check needed?
			if (width == entry.w && height==entry.h && FullFormat == entry.fmt && entry.MipLevels < maxlevel)
			{
				skip_texture_create = true;
			}
			else
			{
				entry.Destroy(false);
				textures.erase(iter);
			}
		}
	}

	// make an entry in the table
	TCacheEntry& entry = textures[texID];
	PC_TexFormat pcfmt = PC_TEX_FMT_NONE;

	pcfmt = TexDecoder_Decode(temp, ptr, expandedWidth, expandedHeight, tex_format, tlutaddr, tlutfmt, true);

	entry.oldpixel = ((u32*)ptr)[0];
	if (g_ActiveConfig.bSafeTextureCache) entry.hash = hash_value;
	else entry.hash = ((u32*)ptr)[0] = (u32)(((double)rand() / RAND_MAX) * 0xFFFFFFFF);

	bool isPow2 = !((width & (width - 1)) || (height & (height - 1)));
	unsigned int TexLevels = (isPow2 && UseNativeMips && maxlevel) ? GetPow2(max(width, height)) : ((isPow2)? 0 : 1);
	if (TexLevels > (maxlevel + 1) && maxlevel)
		TexLevels = maxlevel + 1;

	D3D11_USAGE usage = (TexLevels == 1) ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;

	if (!skip_texture_create)
	{
		// TODO: A little more verbosity in the debug names would be quite helpful..
		D3D11_CPU_ACCESS_FLAG cpu_access = (TexLevels == 1) ? D3D11_CPU_ACCESS_WRITE : (D3D11_CPU_ACCESS_FLAG)0;
		ID3D11Texture2D* pTexture = NULL;
		HRESULT hr;
		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = temp;
		data.SysMemPitch = 4*expandedWidth;

		D3D11_TEXTURE2D_DESC texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, TexLevels, D3D11_BIND_SHADER_RESOURCE, usage, cpu_access);
		hr = D3D::device->CreateTexture2D(&texdesc, (TexLevels==1)?&data:NULL, &pTexture);
		CHECK(hr==S_OK, "Create texture of the TextureCache");
		entry.texture = new D3DTexture2D(pTexture, D3D11_BIND_SHADER_RESOURCE);
		CHECK(entry.texture!=NULL, "Create texture of the TextureCache");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)entry.texture->GetTex(), "a texture of the TextureCache");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)entry.texture->GetSRV(), "shader resource view of a texture of the TextureCache");
		SAFE_RELEASE(pTexture);

		if (TexLevels != 1) D3D::ReplaceRGBATexture2D(entry.texture->GetTex(), temp, width, height, expandedWidth, 0, usage);
	}
	else
	{
		D3D::ReplaceRGBATexture2D(entry.texture->GetTex(), temp, width, height, expandedWidth, 0, usage);
	}
	entry.addr = address;
	entry.size_in_bytes = TexDecoder_GetTextureSizeInBytes(expandedWidth, expandedHeight, tex_format);
	entry.isRenderTarget = false;
	entry.isNonPow2 = false;
	entry.MipLevels = maxlevel;

	if (TexLevels == 0) PD3DX11FilterTexture(D3D::context, entry.texture->GetTex(), 0, D3DX11_DEFAULT);
	else if (TexLevels > 1 && pcfmt != PC_TEX_FMT_NONE)
	{
		unsigned int level = 1;
		unsigned int mipWidth = (width + 1) >> 1;
		unsigned int mipHeight = (height + 1) >> 1;
		ptr += entry.size_in_bytes;
		while ((mipHeight || mipWidth) && (level < TexLevels))
		{
			unsigned int currentWidth = (mipWidth > 0) ? mipWidth : 1;
			unsigned int currentHeight = (mipHeight > 0) ? mipHeight : 1;
			expandedWidth  = (currentWidth + bsw)  & (~bsw);
			expandedHeight = (currentHeight + bsh) & (~bsh);
			TexDecoder_Decode(temp, ptr, expandedWidth, expandedHeight, tex_format, tlutaddr, tlutfmt, true);
			D3D::ReplaceRGBATexture2D(entry.texture->GetTex(), temp, currentWidth, currentHeight, expandedWidth, level, usage);
			u32 size = (max(mipWidth, bsw) * max(mipHeight, bsh) * bsdepth) >> 1;
			ptr +=  size;
			mipWidth >>= 1;
			mipHeight >>= 1;
			level++;
		}
	}
	entry.frameCount = frameCount;
	entry.w = width;
	entry.h = height;
	entry.fmt = FullFormat;

	INCSTAT(stats.numTexturesCreated);
	SETSTAT(stats.numTexturesAlive, (int)textures.size());

	D3D::gfxstate->SetShaderResource(stage, entry.texture->GetSRV());

	return &entry;
}

void TextureCache::CopyRenderTargetToTexture(u32 address, bool bFromZBuffer, bool bIsIntensityFmt, u32 copyfmt, unsigned int bScaleByHalf, const EFBRectangle &source_rect)
{
	int efb_w = source_rect.GetWidth();
	int efb_h = source_rect.GetHeight();

	int tex_w = (abs(source_rect.GetWidth()) >> bScaleByHalf);
	int tex_h = (abs(source_rect.GetHeight()) >> bScaleByHalf);

	int Scaledtex_w = (g_ActiveConfig.bCopyEFBScaled) ? ((int)(Renderer::GetTargetScaleX() * tex_w)) : tex_w;
	int Scaledtex_h = (g_ActiveConfig.bCopyEFBScaled) ? ((int)(Renderer::GetTargetScaleY() * tex_h)) : tex_h;

	TexCache::iterator iter;
	D3DTexture2D* tex = NULL;
	iter = textures.find(address);
	if (iter != textures.end())
	{
		if (iter->second.isRenderTarget && iter->second.Scaledw == Scaledtex_w && iter->second.Scaledh == Scaledtex_h)
		{
			tex = iter->second.texture;
			iter->second.frameCount = frameCount;
		}
		else
		{
			// remove it and recreate it as a render target
			SAFE_RELEASE(iter->second.texture);
			textures.erase(iter);
		}
	}

	if (!tex)
	{
		TCacheEntry entry;
		entry.isRenderTarget = true;
		entry.hash = 0;
		entry.frameCount = frameCount;
		entry.w = tex_w;
		entry.h = tex_h;
		entry.Scaledw = Scaledtex_w;
		entry.Scaledh = Scaledtex_h;
		entry.fmt = copyfmt;
		entry.isNonPow2 = true;
		entry.texture = D3DTexture2D::Create(Scaledtex_w, Scaledtex_h, (D3D11_BIND_FLAG)((int)D3D11_BIND_RENDER_TARGET|(int)D3D11_BIND_SHADER_RESOURCE), D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM);
		if (entry.texture == NULL) PanicAlert("CopyRenderTargetToTexture failed to create entry.texture at %s %d\n", __FILE__, __LINE__);
		textures[address] = entry;
		tex = entry.texture;
	}

	float colmat[20]= {0.0f}; // last four floats for fConstAdd
	unsigned int cbufid = (unsigned int)-1;

	// TODO: Move this to TextureCache::Init()
	if (bFromZBuffer)
	{
		switch(copyfmt)
		{
			case 0: // Z4
			case 1: // Z8
				colmat[0] = colmat[4] = colmat[8] = colmat[12] = 1.0f;
				cbufid = 12;
				break;
			case 3: // Z16 //?
				colmat[1] = colmat[5] = colmat[9] = colmat[12] = 1.0f;
				cbufid = 13;
			case 11: // Z16 (reverse order)
				colmat[0] = colmat[4] = colmat[8] = colmat[13] = 1.0f;
				cbufid = 14;
				break;
			case 6: // Z24X8
				colmat[0] = colmat[5] = colmat[10] = 1.0f;
				cbufid = 15;
				break;
			case 9: // Z8M
				colmat[1] = colmat[5] = colmat[9] = colmat[13] = 1.0f;
				cbufid = 16;
				break;
			case 10: // Z8L
				colmat[2] = colmat[6] = colmat[10] = colmat[14] = 1.0f;
				cbufid = 17;
				break;
			case 12: // Z16L
				colmat[2] = colmat[6] = colmat[10] = colmat[13] = 1.0f;
				cbufid = 18;
				break;
			default:
				ERROR_LOG(VIDEO, "Unknown copy zbuf format: 0x%x", copyfmt);
				colmat[2] = colmat[5] = colmat[8] = 1.0f;
				cbufid = 19;
				break;
		}
	}
	else if (bIsIntensityFmt) 
	{
		colmat[16] = colmat[17] = colmat[18] = 16.0f/255.0f;
		switch (copyfmt) 
		{
			case 0: // I4
			case 1: // I8
			case 2: // IA4
			case 3: // IA8
				// TODO - verify these coefficients
				colmat[0] = 0.257f; colmat[1] = 0.504f; colmat[2] = 0.098f;
				colmat[4] = 0.257f; colmat[5] = 0.504f; colmat[6] = 0.098f;
				colmat[8] = 0.257f; colmat[9] = 0.504f; colmat[10] = 0.098f;

				if (copyfmt < 2) 
				{
					colmat[19] = 16.0f / 255.0f;
					colmat[12] = 0.257f; colmat[13] = 0.504f; colmat[14] = 0.098f;
					cbufid = 0;
				}
				else// alpha
				{
					colmat[15] = 1;
					cbufid = 1;
				}

				break;
			default:
				ERROR_LOG(VIDEO, "Unknown copy intensity format: 0x%x", copyfmt);
				colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1;
				break;
		}
	}
	else
	{
		switch (copyfmt) 
		{
			case 0: // R4
			case 8: // R8
				colmat[0] = colmat[4] = colmat[8] = colmat[12] = 1;
				cbufid = 2;
				break;
			case 2: // RA4
			case 3: // RA8
				colmat[0] = colmat[4] = colmat[8] = colmat[15] = 1;
				cbufid = 3;
				break;

			case 7: // A8
				colmat[3] = colmat[7] = colmat[11] = colmat[15] = 1; 
				cbufid = 4;
				break;
			case 9: // G8
				colmat[1] = colmat[5] = colmat[9] = colmat[13] = 1;
				cbufid = 5;
				break;
			case 10: // B8
				colmat[2] = colmat[6] = colmat[10] = colmat[14] = 1;
				cbufid = 6;
				break;
			case 11: // RG8
				colmat[0] = colmat[4] = colmat[8] = colmat[13] = 1;
				cbufid = 7;
				break;
			case 12: // GB8
				colmat[1] = colmat[5] = colmat[9] = colmat[14] = 1;
				cbufid = 8;
				break;

			case 4: // RGB565
				colmat[0] = colmat[5] = colmat[10] = 1;
				colmat[19] = 1; // set alpha to 1
				cbufid = 9;
				break;
			case 5: // RGB5A3
			case 6: // RGBA8
				colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1;
				cbufid = 10;
				break;

			default:
				ERROR_LOG(VIDEO, "Unknown copy color format: 0x%x", copyfmt);
				colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1;
				cbufid = 11;
				break;
		}
	}

	Renderer::ResetAPIState(); // reset any game specific settings
	
	// stretch picture with increased internal resolution
	D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)Scaledtex_w, (float)Scaledtex_h);
	D3D::context->RSSetViewports(1, &vp);
	D3D11_RECT destrect = CD3D11_RECT(0, 0, Scaledtex_w, Scaledtex_h);

	// set transformation
	if (efbcopycbuf[cbufid] == NULL)
	{
		D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(20*sizeof(float), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DEFAULT);
		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = colmat;
		HRESULT hr = D3D::device->CreateBuffer(&cbdesc, &data, &efbcopycbuf[cbufid]);
		CHECK(hr==S_OK, "Create efb copy constant buffer %d", cbufid);
		D3D::SetDebugObjectName((ID3D11DeviceChild*)efbcopycbuf[cbufid], "a constant buffer used in TextureCache::CopyRenderTargetToTexture");
	}
	D3D::context->PSSetConstantBuffers(0, 1, &efbcopycbuf[cbufid]);

	TargetRectangle targetSource = Renderer::ConvertEFBRectangle(source_rect);
	D3D11_RECT sourcerect = CD3D11_RECT(targetSource.left, targetSource.top, targetSource.right, targetSource.bottom);

	// Use linear filtering if (bScaleByHalf), use point filtering otherwise
	if (bScaleByHalf) D3D::SetLinearCopySampler();
	else D3D::SetPointCopySampler();

	D3D::stateman->PushBlendState(efbcopyblendstate);
	D3D::stateman->PushRasterizerState(efbcopyraststate);
	D3D::stateman->PushDepthState(efbcopydepthstate);
	D3D::context->OMSetRenderTargets(1, &tex->GetRTV(), NULL);
	D3D::drawShadedTexQuad(
				(bFromZBuffer) ? FBManager.GetEFBDepthTexture()->GetSRV() : FBManager.GetEFBColorTexture()->GetSRV(),
				&sourcerect,
				Renderer::GetFullTargetWidth(),
				Renderer::GetFullTargetHeight(),
				(bFromZBuffer) ? PixelShaderCache::GetDepthMatrixProgram() : PixelShaderCache::GetColorMatrixProgram(), VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout());

	D3D::context->OMSetRenderTargets(1, &FBManager.GetEFBColorTexture()->GetRTV(), FBManager.GetEFBDepthTexture()->GetDSV());
	D3D::stateman->PopBlendState();
	D3D::stateman->PopDepthState();
	D3D::stateman->PopRasterizerState();
	Renderer::RestoreAPIState();
}
