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

#include "TextureCache.h"

#include "VideoConfig.h"
#include "D3DBase.h"
#include "D3DTexture.h"
#include "D3DShader.h"
#include "PSTextureEncoder.h"
#include "VertexShaderCache.h"
#include "FramebufferManager.h"
#include "Render.h"
#include "TexCopyLookaside.h"
#include "LookUpTables.h"
#include "GfxState.h"
#include "HW/Memmap.h"

namespace DX11
{

TCacheEntry::TCacheEntry()
	: m_inTmem(false), m_fromTcl(false), m_loaded(NULL), m_depalettized(NULL), m_bindMe(NULL)
{ }

void TCacheEntry::EvictFromTmem()
{
	m_inTmem = false;
}

inline bool IsPaletted(u32 format)
{
	return format == GX_TF_C4 || format == GX_TF_C8 || format == GX_TF_C14X2;
}

// Refresh steps:
// Load -> Depalettize -> Bind

// Load:
// If TCL is available at address, try to fake a texture from it. If fake
// failed or there is no TCL, load from RAM.

// Depalettize:
// If format is palettized, refresh TLUT.
// If format is not palettized, just pass on loaded texture.

// Bind:
// Texture is ready for binding.

// Compute the maximum number of mips a texture of given dims could have
// Some games (Luigi's Mansion for example) try to use too many mip levels.
static unsigned int ComputeMaxLevels(unsigned int width, unsigned int height)
{
	if (width == 0 || height == 0)
		return 0;
	unsigned int max = std::max(width, height);
	unsigned int levels = 0;
	while (max > 0)
	{
		++levels;
		max /= 2;
	}
	return levels;
}

void TCacheEntry::Refresh(u32 ramAddr, u32 width, u32 height, u32 levels,
	u32 format, u32 tlutAddr, u32 tlutFormat)
{
	m_fromTcl = false;
	m_loaded = NULL;
	m_loadedDirty = false;
	m_depalettized = NULL;
	m_bindMe = NULL;

	// This is the earliest possible place to correct mip levels. Do so.
	unsigned int maxLevels = ComputeMaxLevels(width, height);
	levels = std::min(levels, maxLevels);

	Load(ramAddr, width, height, levels, format, tlutAddr, tlutFormat);
	Depalettize(ramAddr, width, height, levels, format, tlutAddr, tlutFormat);

	m_bindMe = m_depalettized;
}

void TCacheEntry::Bind(int stage)
{
	if (m_bindMe)
		D3D::g_context->PSSetShaderResources(stage, 1, &m_bindMe->GetSRV());
	else
		D3D::g_context->PSSetShaderResources(stage, 0, NULL);
}

void TCacheEntry::Load(u32 ramAddr, u32 width, u32 height, u32 levels,
	u32 format, u32 tlutAddr, u32 tlutFormat)
{
	bool refreshFromRam = true;
	
	// See if there is a TCL available at this address
	TexCopyLookasideMap& tclMap = ((TextureCache*)g_textureCache)->GetTclMap();
	TexCopyLookasideMap::iterator tclIt = tclMap.find(ramAddr);

	if (tclIt != tclMap.end())
	{
		if (g_ActiveConfig.bEFBCopyVirtualEnable)
		{
			// Try to load from the TCL instead of RAM.
			// FIXME: If TCL keeps failing to load, its memory region may have
			// been deallocated by the game. We ought to delete TCL that aren't
			// being used.
			if (LoadFromTcl(ramAddr, width, height, levels, format, tlutAddr, tlutFormat, tclIt->second.get()))
				refreshFromRam = false;
		}
		else
		{
			// User must have turned off virtual copies mid-game. Delete any
			// that are found.
			// FIXME: Is it desirable to delete them as they are found instead
			// of just deleting them all at once?
			tclMap.erase(tclIt);
		}
	}

	if (refreshFromRam)
		LoadFromRam(ramAddr, width, height, levels, format, tlutAddr, tlutFormat);
}

void TCacheEntry::LoadFromRam(u32 ramAddr, u32 width, u32 height, u32 levels,
	u32 format, u32 tlutAddr, u32 tlutFormat)
{
	const u8* src = Memory::GetPointer(ramAddr);

	// Would real hardware reload data from RAM to TMEM?
	bool loadToTmem = !m_inTmem || ramAddr != m_ramAddr;

	bool dimsChanged = width != m_curWidth || height != m_curHeight || levels != m_curLevels;

	// Does changing format require creating a new texture?
	bool formatRequiresRecreate = (IsPaletted(format) != IsPaletted(m_curFormat))
		|| (IsPaletted(format) && ((format == GX_TF_C14X2) != (m_curFormat == GX_TF_C14X2)));

	// Should we create a new D3D texture?
	bool recreateTexture = !m_ramTexture || dimsChanged || formatRequiresRecreate;

	if (recreateTexture)
		CreateRamTexture(ramAddr, width, height, levels, format, tlutAddr, tlutFormat);

	u64 newHash = m_curHash;

	bool reloadTexture = recreateTexture || format != m_curFormat;
	if (reloadTexture || loadToTmem)
	{
		// FIXME: Only the top-level mip is hashed.
		u32 sizeInBytes = TexDecoder_GetTextureSizeInBytes(width, height, format);
		newHash = GetHash64(src, sizeInBytes, sizeInBytes);
		reloadTexture |= (newHash != m_curHash);
		DEBUG_LOG(VIDEO, "Hash of texture at 0x%.08X was taken... 0x%.016X", ramAddr, newHash);
	}

	if (reloadTexture)
		ReloadRamTexture(ramAddr, width, height, levels, format, tlutAddr, tlutFormat);

	m_inTmem = true;
	m_ramAddr = ramAddr;

	m_curWidth = width;
	m_curHeight = height;
	m_curLevels = levels;
	m_curFormat = format;
	m_curHash = newHash;

	m_fromTcl = false;
	m_loaded = m_ramTexture.get();
	m_loadedDirty = reloadTexture;
}

void TCacheEntry::CreateRamTexture(u32 ramAddr, u32 width, u32 height, u32 levels,
	u32 format, u32 tlutAddr, u32 tlutFormat)
{
	if (IsPaletted(format))
	{
		INFO_LOG(VIDEO, "Creating %dx%d %d-level format 0x%X texture at 0x%.08X with tlut at 0x%.05X tlut-format %d",
			width, height, levels, format, ramAddr, tlutAddr, tlutFormat);

		// Create texture with color indices (palette is applied later)

		D3D11_TEXTURE2D_DESC t2dd = CD3D11_TEXTURE2D_DESC(
			(format == GX_TF_C14X2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R8_UINT),
			width, height, 1, levels);

		m_ramTexture.reset();
		SharedPtr<ID3D11Texture2D> newRamTexture = CreateTexture2DShared(&t2dd, NULL);
		m_ramTexture.reset(new D3DTexture2D(newRamTexture, D3D11_BIND_SHADER_RESOURCE));
	}
	else
	{
		INFO_LOG(VIDEO, "Creating %dx%d %d-level format 0x%X texture at 0x%.08X",
			width, height, levels, format, ramAddr);

		D3D11_TEXTURE2D_DESC t2dd = CD3D11_TEXTURE2D_DESC(
			DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, levels);

		m_ramTexture.reset();
		SharedPtr<ID3D11Texture2D> newRamTexture = CreateTexture2DShared(&t2dd, NULL);
		m_ramTexture.reset(new D3DTexture2D(newRamTexture, D3D11_BIND_SHADER_RESOURCE));
	}
}

static void DecodeC14X2Base(u8* dst, const u8* src, u32 width, u32 height)
{
	int Wsteps4 = (width + 3) / 4;

	for (int y = 0; y < height; y += 4)
	{
		for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
		{
			for (int iy = 0, xStep = yStep * 4; iy < 4; iy++, xStep++)
			{
				u16 *ptr = (u16 *)dst + (y + iy) * width + x;
				u16 *s = (u16 *)(src + 8 * xStep);
				for(int j = 0; j < 4; j++)
					*ptr++ = Common::swap16(*s++);
			}
		}
	}
}

static void DecodeC8Base(u8* dst, const u8* src, u32 width, u32 height)
{
	int Wsteps8 = (width + 7) / 8;

	for (int y = 0; y < height; y += 4)
	{
		for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
		{
			for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
			{
				((u64*)(dst + (y + iy) * width + x))[0] = ((u64*)(src + 8 * xStep))[0];
			}
		}
	}
}

static void DecodeC4Base(u8* dst, const u8* src, u32 width, u32 height)
{
	int Wsteps8 = (width + 7) / 8;

	for (int y = 0; y < height; y += 8)
	{
		for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8, yStep++)
		{
			for (int iy = 0, xStep = yStep * 8 ; iy < 8; iy++,xStep++)
			{
				for (int ix = 0; ix < 4; ix++)
				{
					int val = src[4 * xStep + ix];
					dst[(y + iy) * width + x + ix * 2] = val >> 4;
					dst[(y + iy) * width + x + ix * 2 + 1] = val & 0xF;
				}
			}
		}
	}
}

void TCacheEntry::ReloadRamTexture(u32 ramAddr, u32 width, u32 height, u32 levels,
	u32 format, u32 tlutAddr, u32 tlutFormat)
{
	int blockW = TexDecoder_GetBlockWidthInTexels(format);
	int blockH = TexDecoder_GetBlockHeightInTexels(format);

	const u8* src = Memory::GetPointer(ramAddr);

	if (IsPaletted(format))
	{
		DEBUG_LOG(VIDEO, "Loading texture format 0x%X with tlut at 0x%.05X tlut-format %d",
			format, tlutAddr, tlutFormat);

		// FIXME: Are mipmapped palettized textures even supported?

		u32 mipWidth = width;
		u32 mipHeight = height;
		u32 level = 0;
		while ((mipWidth > 0 && mipHeight > 0) && level < levels)
		{
			int actualWidth = (mipWidth + blockW-1) & ~(blockW-1);
			int actualHeight = (mipHeight + blockH-1) & ~(blockH-1);

			u8* decodeTemp = ((TextureCache*)g_textureCache)->GetDecodeTemp();
			if (format == GX_TF_C14X2)
			{
				// 16-bit indices
				DecodeC14X2Base(decodeTemp, src, actualWidth, actualHeight);
				D3D::g_context->UpdateSubresource(m_ramTexture->GetTex(), level,
					NULL, decodeTemp, 2*actualWidth, 2*actualWidth*actualHeight);
			}
			else if (format == GX_TF_C8)
			{
				// 8-bit indices
				DecodeC8Base(decodeTemp, src, actualWidth, actualHeight);
				D3D::g_context->UpdateSubresource(m_ramTexture->GetTex(), level,
					NULL, decodeTemp, actualWidth, actualWidth*actualHeight);
			}
			else if (format == GX_TF_C4)
			{
				// 4-bit indices (expanded to 8 bits)
				DecodeC4Base(decodeTemp, src, actualWidth, actualHeight);
				D3D::g_context->UpdateSubresource(m_ramTexture->GetTex(), level,
					NULL, decodeTemp, actualWidth, actualWidth*actualHeight);
			}

			src += TexDecoder_GetTextureSizeInBytes(mipWidth, mipHeight, format);

			mipWidth /= 2;
			mipHeight /= 2;
			++level;
		}
	}
	else
	{
		DEBUG_LOG(VIDEO, "Loading texture format 0x%X", format);

		// FIXME: Hash all the mips seperately?

		u32 mipWidth = width;
		u32 mipHeight = height;
		u32 level = 0;
		while ((mipWidth > 0 && mipHeight > 0) && level < levels)
		{
			int actualWidth = (mipWidth + blockW-1) & ~(blockW-1);
			int actualHeight = (mipHeight + blockH-1) & ~(blockH-1);

			u8* decodeTemp = ((TextureCache*)g_textureCache)->GetDecodeTemp();
			TexDecoder_Decode(decodeTemp, src, actualWidth, actualHeight, format, NULL, tlutFormat, true);
			D3D::g_context->UpdateSubresource(m_ramTexture->GetTex(), level, NULL,
				decodeTemp, 4*actualWidth, 4*actualWidth*actualHeight);

			src += TexDecoder_GetTextureSizeInBytes(mipWidth, mipHeight, format);

			mipWidth /= 2;
			mipHeight /= 2;
			++level;
		}
	}
}

bool TCacheEntry::LoadFromTcl(u32 ramAddr, u32 width, u32 height, u32 levels,
	u32 format, u32 tlutAddr, u32 tlutFormat, TexCopyLookaside* tcl)
{
	if (g_ActiveConfig.bEFBCopyRAMEnable)
	{
		// Only make these checks if there is a RAM copy to fall back on.
		if (width != tcl->GetRealWidth() || height != tcl->GetRealHeight() || levels != 1)
		{
			INFO_LOG(VIDEO, "Tex-copy lookaside was incompatible; falling back to RAM");
			return false;
		}
	}

	// Would real hardware reload data from RAM to TMEM?
	bool loadToTmem = !m_inTmem || ramAddr != m_ramAddr;

	u64 newHash = m_curHash;

	// If texture will be loaded to TMEM, make sure mem hash matches TCL hash.
	// Otherwise, it's acceptable to just use the fake texture as is.
	if (loadToTmem)
	{
		const u8* src = Memory::GetPointer(ramAddr);

		if (g_ActiveConfig.bEFBCopyRAMEnable)
		{
			// We made a RAM copy, so check its hash for changes
			u32 sizeInBytes = TexDecoder_GetTextureSizeInBytes(width, height, format);
			newHash = GetHash64(src, sizeInBytes, sizeInBytes);
			DEBUG_LOG(VIDEO, "Hash of TCL'ed texture at 0x%.08X was taken... 0x%.016X", ramAddr, newHash);
		}
		else
		{
			// We must be doing virtual copies only, so look for canary data.
			newHash = *(u64*)src;
		}

		if (newHash != tcl->GetHash())
		{
			INFO_LOG(VIDEO, "EFB copy may have been modified since encoding; falling back to RAM");
			return false;
		}
	}

	D3DTexture2D* fakeTexture = tcl->FakeTexture(ramAddr, width, height, levels, format, tlutAddr, tlutFormat);
	if (!fakeTexture)
		return false;

	m_ramTexture.reset();

	m_inTmem = true;
	m_ramAddr = ramAddr;

	m_curWidth = width;
	m_curHeight = height;
	m_curLevels = levels;
	m_curFormat = format;
	m_curHash = newHash;

	m_fromTcl = true;
	m_loaded = fakeTexture;
	m_loadedDirty = tcl->IsDirty();
	tcl->ResetDirty();
	return true;
}

void TCacheEntry::Depalettize(u32 ramAddr, u32 width, u32 height, u32 levels,
	u32 format, u32 tlutAddr, u32 tlutFormat)
{
	if (IsPaletted(format))
	{
		bool paletteChanged = RefreshTlut(ramAddr, width, height, levels, format, tlutAddr, tlutFormat);

		D3D11_TEXTURE2D_DESC loadedDesc;
		m_loaded->GetTex()->GetDesc(&loadedDesc);

		bool recreateDepal = !m_depalStorage.tex ||
			loadedDesc.Width != m_depalStorage.width ||
			loadedDesc.Height != m_depalStorage.height ||
			loadedDesc.MipLevels != m_depalStorage.levels;

		if (recreateDepal)
		{
			// Create depalettized texture storage (a shader is used to translate loaded to depalettizedStorage)

			D3D11_TEXTURE2D_DESC t2dd = CD3D11_TEXTURE2D_DESC(
				DXGI_FORMAT_R8G8B8A8_UNORM, loadedDesc.Width, loadedDesc.Height, 1, loadedDesc.MipLevels,
				D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

			m_depalStorage.tex.reset();
			SharedPtr<ID3D11Texture2D> newDepal = CreateTexture2DShared(&t2dd, NULL);
			m_depalStorage.tex.reset(new D3DTexture2D(newDepal,
				(D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET)));
		}

		bool runDepalShader = recreateDepal || m_loadedDirty || paletteChanged;
		if (runDepalShader)
			DepalettizeShader(ramAddr, width, height, levels, format, tlutAddr, tlutFormat);

		m_depalettized = m_depalStorage.tex.get();
	}
	else
	{
		m_depalStorage.tex.reset();
		m_paletteSRV.reset();
		m_palette.reset();
		m_depalettized = m_loaded;
	}
}

static void DecodeIA8Palette(u32* dst, const u16* src, unsigned int numColors)
{
	for (unsigned int i = 0; i < numColors; ++i)
	{
		u8 intensity = src[i] & 0xFF;
		u8 alpha = src[i] >> 8;
		dst[i] = (alpha << 24) | (intensity << 16) | (intensity << 8) | intensity;
	}
}

static void DecodeRGB565Palette(u32* dst, const u16* src, unsigned int numColors)
{
	for (unsigned int i = 0; i < numColors; ++i)
	{
		u16 color = Common::swap16(src[i]);
		u8 red8 = Convert5To8(color >> 11);
		u8 green8 = Convert6To8((color >> 5) & 0x3F);
		u8 blue8 = Convert5To8(color & 0x1F);
		dst[i] = (0xFF << 24) | (blue8 << 16) | (green8 << 8) | red8;
	}
}

static inline u32 decode5A3(u16 val)
{
	int r,g,b,a;
	if ((val & 0x8000))
	{
		a = 0xFF;
		r = Convert5To8((val >> 10) & 0x1F);
		g = Convert5To8((val >> 5) & 0x1F);
		b = Convert5To8(val & 0x1F);
	}
	else
	{
		a = Convert3To8((val >> 12) & 0x7);
		r = Convert4To8((val >> 8) & 0xF);
		g = Convert4To8((val >> 4) & 0xF);
		b = Convert4To8(val & 0xF);
	}
	return (a << 24) | (b << 16) | (g << 8) | r;
}

static void DecodeRGB5A3Palette(u32* dst, const u16* src, unsigned int numColors)
{
	for (unsigned int i = 0; i < numColors; ++i)
	{
		dst[i] = decode5A3(Common::swap16(src[i]));
	}
}

bool TCacheEntry::RefreshTlut(u32 ramAddr, u32 width, u32 height, u32 levels,
	u32 format, u32 tlutAddr, u32 tlutFormat)
{
	if (IsPaletted(format))
	{
		bool recreatePaletteTex = !m_palette || !m_paletteSRV || format != m_lastPalettedFormat;
		if (recreatePaletteTex)
			CreatePaletteTexture(ramAddr, width, height, levels, format, tlutAddr, tlutFormat);

		const u16* tlut = (const u16*)&g_textureCache->GetCache()[tlutAddr];
		u32 paletteSize = TexDecoder_GetPaletteSize(format);
		u64 newPaletteHash = GetHash64((const u8*)tlut, paletteSize, paletteSize);
		DEBUG_LOG(VIDEO, "Hash of tlut at 0x%.05X was taken... 0x%.016X", tlutAddr, newPaletteHash);

		bool reloadPalette = recreatePaletteTex || tlutFormat != m_curTlutFormat || newPaletteHash != m_curPaletteHash;
		if (reloadPalette)
		{
			unsigned int numColors = TexDecoder_GetNumColors(format);

			D3D11_MAPPED_SUBRESOURCE map = { 0 };
			HRESULT hr = D3D::g_context->Map(m_palette, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
			if (SUCCEEDED(hr))
			{
				switch (tlutFormat)
				{
				case GX_TL_IA8: DecodeIA8Palette((u32*)map.pData, tlut, numColors); break;
				case GX_TL_RGB565: DecodeRGB565Palette((u32*)map.pData, tlut, numColors); break;
				case GX_TL_RGB5A3: DecodeRGB5A3Palette((u32*)map.pData, tlut, numColors); break;
				}
				D3D::g_context->Unmap(m_palette, 0);
			}
			else
				ERROR_LOG(VIDEO, "Failed to map palette texture");
		}

		m_curPaletteHash = newPaletteHash;
		m_curTlutFormat = tlutFormat;
		m_lastPalettedFormat = format;

		return reloadPalette;
	}
	else
		return false;
}

void TCacheEntry::CreatePaletteTexture(u32 ramAddr, u32 width, u32 height, u32 levels,
	u32 format, u32 tlutAddr, u32 tlutFormat)
{
	unsigned int numColors = TexDecoder_GetNumColors(format);

	D3D11_TEXTURE1D_DESC t1dd = CD3D11_TEXTURE1D_DESC(
		DXGI_FORMAT_R8G8B8A8_UNORM, numColors, 1, 1, D3D11_BIND_SHADER_RESOURCE,
		D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

	ID3D11Texture1D* newPalette = NULL;
	HRESULT hr = D3D::g_device->CreateTexture1D(&t1dd, NULL, &newPalette);
	if (FAILED(hr))
		ERROR_LOG(VIDEO, "Failed to create new palette texture");
	m_palette = SharedPtr<ID3D11Texture1D>::FromPtr(newPalette);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvd = CD3D11_SHADER_RESOURCE_VIEW_DESC(
		m_palette, D3D11_SRV_DIMENSION_TEXTURE1D);
	ID3D11ShaderResourceView* newPaletteSRV = NULL;
	hr = D3D::g_device->CreateShaderResourceView(m_palette, &srvd, &newPaletteSRV);
	if (FAILED(hr))
		ERROR_LOG(VIDEO, "Failed to create new palette texture SRV");
	m_paletteSRV = SharedPtr<ID3D11ShaderResourceView>::FromPtr(newPaletteSRV);
}

void TCacheEntry::DepalettizeShader(u32 ramAddr, u32 width, u32 height, u32 levels,
	u32 format, u32 tlutAddr, u32 tlutFormat)
{
	DEBUG_LOG(VIDEO, "Depalettizing texture with new TLUT");

	// Reset API

	g_renderer->ResetAPIState();
	D3D::stateman->Apply();

	D3D11_TEXTURE2D_DESC loadedDesc;
	m_loaded->GetTex()->GetDesc(&loadedDesc);

	D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, FLOAT(loadedDesc.Width), FLOAT(loadedDesc.Height));
	D3D::g_context->RSSetViewports(1, &vp);

	// Re-encode with a different palette

	SharedPtr<ID3D11PixelShader> depalettizeShader;
	if (m_fromTcl)
	{
		if (format == GX_TF_C4)
			depalettizeShader = ((TextureCache*)g_textureCache)->GetDepal4Shader();
		else if (format == GX_TF_C8)
			depalettizeShader = ((TextureCache*)g_textureCache)->GetDepal8Shader();
		else if (format == GX_TF_C14X2)
		{
			// TODO: When would this happen and how would we handle it?
			depalettizeShader = ((TextureCache*)g_textureCache)->GetDepal8Shader();
			ERROR_LOG(VIDEO, "Not implemented: Reinterpret TCL as C14X2!");
		}
	}
	else
		depalettizeShader = ((TextureCache*)g_textureCache)->GetDepalUintShader();

	D3D::g_context->OMSetRenderTargets(1, &m_depalStorage.tex->GetRTV(), NULL);
	D3D::g_context->PSSetShaderResources(8, 1, &m_loaded->GetSRV());
	D3D::g_context->PSSetShaderResources(9, 1, &m_paletteSRV);

	D3D::drawEncoderQuad(depalettizeShader);

	ID3D11ShaderResourceView* nullDummy = NULL;
	D3D::g_context->PSSetShaderResources(8, 1, &nullDummy);
	D3D::g_context->PSSetShaderResources(9, 1, &nullDummy);
	
	// Restore API

	g_renderer->RestoreAPIState();
	D3D::g_context->OMSetRenderTargets(1,
		&FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());
}

TextureCache::TextureCache()
{
	m_encoder.reset(new PSTextureEncoder);
}

void TextureCache::EncodeEFB(u32 dstAddr, unsigned int dstFormat,
	unsigned int srcFormat, const EFBRectangle& srcRect, bool isIntensity,
	bool scaleByHalf)
{
	u8* dst = Memory::GetPointer(dstAddr);

	u64 encodedHash = 0;
	if (g_ActiveConfig.bEFBCopyRAMEnable)
	{
		// Take the hash of the encoded data to detect if the game overwrites
		// it.
		size_t encodeSize = m_encoder->Encode(dst, dstFormat, srcFormat, srcRect, isIntensity, scaleByHalf);
		if (encodeSize)
		{
			encodedHash = GetHash64(dst, encodeSize, encodeSize);
			DEBUG_LOG(VIDEO, "Hash of EFB copy at 0x%.08X was taken... 0x%.016X", dstAddr, encodedHash);
		}
	}
	else if (g_ActiveConfig.bEFBCopyVirtualEnable)
	{
		static u64 canaryEgg = 0x79706F4342464500; // '\0EFBCopy'

		// We aren't encoding to RAM but we are making a TCL, so put a piece of
		// canary data in RAM to detect if the game overwrites it.
		encodedHash = canaryEgg;
		++canaryEgg;

		// There will be at least 32 bytes that are safe to write here.
		*(u64*)dst = encodedHash;
	}

	if (g_ActiveConfig.bEFBCopyVirtualEnable)
	{
		TexCopyLookasideMap::iterator tclIt = m_tclMap.find(dstAddr);
		if (tclIt == m_tclMap.end())
		{
			tclIt = m_tclMap.insert(std::make_pair(dstAddr, new TexCopyLookaside)).first;
		}

		TexCopyLookaside* tcl = tclIt->second.get();

		tcl->Update(dstAddr, dstFormat, srcFormat, srcRect, isIntensity, scaleByHalf);
		tcl->SetHash(encodedHash);
	}
}

static const char DEPALETTIZE4_SHADER[] =
"// dolphin-emu depalettizing shader for 4-bit normalized-float indices\n"

// Use t8 and t9 because t0..7 are being used by the vertex manager
"Texture2D Base : register(t8);\n"
"Texture1D<float4> Palette : register(t9);\n"

"void main(out float4 ocol0 : SV_Target, in float4 pos : SV_Position)\n"
"{\n"
	"float sample = Base.Load(int3(pos.x, pos.y, 0)).r;\n"
	"ocol0 = Palette.Load(int2(sample * 15, 0));\n"
"}\n"
;

SharedPtr<ID3D11PixelShader> TextureCache::GetDepal4Shader()
{
	if (!m_depal4Shader)
	{
		m_depal4Shader = D3D::CompileAndCreatePixelShader(DEPALETTIZE4_SHADER, sizeof(DEPALETTIZE4_SHADER));
		if (!m_depal4Shader)
			ERROR_LOG(VIDEO, "Failed to create depalettizing shader");
	}

	return m_depal4Shader;
}

static const char DEPALETTIZE8_SHADER[] =
"// dolphin-emu depalettizing shader for 8-bit normalized-float indices\n"

// Use t8 and t9 because t0..7 are being used by the vertex manager
"Texture2D Base : register(t8);\n"
"Texture1D<float4> Palette : register(t9);\n"

"void main(out float4 ocol0 : SV_Target, in float4 pos : SV_Position)\n"
"{\n"
	"float sample = Base.Load(int3(pos.x, pos.y, 0)).r;\n"
	"ocol0 = Palette.Load(int2(sample * 255, 0));\n"
"}\n"
;

SharedPtr<ID3D11PixelShader> TextureCache::GetDepal8Shader()
{
	if (!m_depal8Shader)
	{
		m_depal8Shader = D3D::CompileAndCreatePixelShader(DEPALETTIZE8_SHADER, sizeof(DEPALETTIZE8_SHADER));
		if (!m_depal8Shader)
			ERROR_LOG(VIDEO, "Failed to create depalettizing shader");
	}

	return m_depal8Shader;
}

static const char DEPALETTIZE_UINT_SHADER[] =
"// dolphin-emu depalettizing shader for uint indices\n"

// Use t8 and t9 because t0..7 are being used by the vertex manager
"Texture2D<uint> Base : register(t8);\n"
"Texture1D<float4> Palette : register(t9);\n"

"void main(out float4 ocol0 : SV_Target, in float4 pos : SV_Position)\n"
"{\n"
	"uint index = Base.Load(int3(pos.x, pos.y, 0));\n"
	"ocol0 = Palette.Load(int2(index, 0));\n"
"}\n"
;

SharedPtr<ID3D11PixelShader> TextureCache::GetDepalUintShader()
{
	if (!m_depalUintShader)
	{
		m_depalUintShader = D3D::CompileAndCreatePixelShader(DEPALETTIZE_UINT_SHADER, sizeof(DEPALETTIZE_UINT_SHADER));
		if (!m_depalUintShader)
			ERROR_LOG(VIDEO, "Failed to create depalettizing shader");
	}

	return m_depalUintShader;
}

TCacheEntry* TextureCache::CreateEntry()
{
	return new TCacheEntry;
}

}
