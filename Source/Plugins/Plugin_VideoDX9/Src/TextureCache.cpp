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

#include "D3DTexture.h"
#include "D3DUtil.h"
#include "FramebufferManager.h"
#include "Hash.h"
#include "HW/Memmap.h"
#include "Render.h"
#include "TextureConverter.h"
#include "Tmem.h"
#include "VideoConfig.h"

namespace DX9
{

#define SAFE_RELEASE(p) if (p) { (p)->Release(); (p) = NULL; }

TCacheEntry::TCacheEntry()
	: m_texture(NULL)
{ }

TCacheEntry::~TCacheEntry()
{
	SAFE_RELEASE(m_texture);
}

inline bool IsPaletted(u32 format)
{
	return format == GX_TF_C4 || format == GX_TF_C8 || format == GX_TF_C14X2;
}

void TCacheEntry::RefreshInternal(u32 ramAddr, u32 width, u32 height, u32 levels,
	u32 format, u32 tlutAddr, u32 tlutFormat, bool invalidated)
{
	int blockW = TexDecoder_GetBlockWidthInTexels(format);
	int blockH = TexDecoder_GetBlockHeightInTexels(format);

	const u8* src = Memory::GetPointer(ramAddr);
	const u16* tlut = (const u16*)&g_texMem[tlutAddr];

	bool dimsChanged = width != m_curWidth || height != m_curHeight || levels != m_curLevels;

	PC_TexFormat pcFormat = GetPC_TexFormat(format, tlutFormat);
	D3DFORMAT d3dFormat = D3DFMT_UNKNOWN;
	bool swapRB = false;
	switch (pcFormat)
	{
	case PC_TEX_FMT_BGRA32:
		d3dFormat = D3DFMT_A8R8G8B8;
		break;
	case PC_TEX_FMT_RGBA32:
		d3dFormat = D3DFMT_A8R8G8B8;
		swapRB = true;
		break;
	case PC_TEX_FMT_I4_AS_I8:
	case PC_TEX_FMT_I8:
		d3dFormat = D3DFMT_A8P8; // Causes ReplaceTexture2D to convert from I8 to A8L8
		break;
	case PC_TEX_FMT_IA4_AS_IA8:
	case PC_TEX_FMT_IA8:
		d3dFormat = D3DFMT_A8L8;
		break;
	case PC_TEX_FMT_RGB565:
		d3dFormat = D3DFMT_R5G6B5;
		break;
	case PC_TEX_FMT_DXT1:
		d3dFormat = D3DFMT_DXT1;
		break;
	default:
		ERROR_LOG(VIDEO, "Unknown PC format for texture format %d", format);
		return;
	}

	// Do we need to create a new D3D texture?
	bool recreateTexture = !m_texture || dimsChanged || d3dFormat != m_curD3DFormat;

	if (recreateTexture)
		CreateRamTexture(width, height, levels, d3dFormat);

	// Do we need to refresh the palette?
	u64 newPaletteHash = m_curPaletteHash;
	if (IsPaletted(format))
	{
		u32 paletteSize = TexDecoder_GetPaletteSize(format);
		newPaletteHash = GetHash64((const u8*)tlut, paletteSize, paletteSize);
	}

	u64 newHash = m_curHash;

	// TODO: Use a shader to depalettize textures, like the DX11 plugin
	bool reloadTexture = recreateTexture || dimsChanged || format != m_curFormat ||
		newPaletteHash != m_curPaletteHash || tlutFormat != m_curTlutFormat;
	if (reloadTexture || invalidated)
	{
		// FIXME: Only the top-level mip is hashed.
		// TODO: Support auto-generating mips
		u32 sizeInBytes = TexDecoder_GetTextureSizeInBytes(width, height, format);
		newHash = GetHash64(src, sizeInBytes, sizeInBytes);
		reloadTexture |= (newHash != m_curHash);
	}

	if (reloadTexture)
	{
		// FIXME: Hash all the mips seperately?
		u32 mipWidth = width;
		u32 mipHeight = height;
		u32 level = 0;
		while ((mipWidth > 0 && mipHeight > 0) && level < levels)
		{
			int actualWidth = (mipWidth + blockW-1) & ~(blockW-1);
			int actualHeight = (mipHeight + blockH-1) & ~(blockH-1);

			u8* decodeTemp = ((TextureCache*)g_textureCache)->GetDecodeTemp();
			PC_TexFormat pcFormat = TexDecoder_Decode(decodeTemp, src,
				actualWidth, actualHeight, format, tlut, tlutFormat, false);

			// Load decoded texture to graphics RAM
			D3D::ReplaceTexture2D(m_texture, decodeTemp, mipWidth, mipHeight,
				actualWidth, d3dFormat, swapRB, level);

			src += TexDecoder_GetTextureSizeInBytes(mipWidth, mipHeight, format);

			mipWidth /= 2;
			mipHeight /= 2;
			++level;
		}
	}

	m_curWidth = width;
	m_curHeight = height;
	m_curLevels = levels;
	m_curFormat = format;
	m_curHash = newHash;

	m_curPaletteHash = newPaletteHash;
	m_curTlutFormat = tlutFormat;

	m_curD3DFormat = d3dFormat;
}

void TCacheEntry::CreateRamTexture(u32 width, u32 height, u32 levels, D3DFORMAT d3dFormat)
{
	SAFE_RELEASE(m_texture);
	m_texture = D3D::CreateOnlyTexture2D(width, height, levels,
		d3dFormat == D3DFMT_A8P8 ? D3DFMT_A8L8 : d3dFormat);
}

void TextureCache::EncodeEFB(u32 dstAddr, unsigned int dstFormat, unsigned int srcFormat,
	const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf)
{
	if (g_ActiveConfig.bEFBCopyRAMEnable)
	{
		u8* dst = Memory::GetPointer(dstAddr);

		TextureConverter::EncodeToRam(dst, dstFormat, srcFormat, srcRect,
			isIntensity, scaleByHalf);
	}
}

TCacheEntry* TextureCache::CreateEntry()
{
	return new TCacheEntry;
}

}
