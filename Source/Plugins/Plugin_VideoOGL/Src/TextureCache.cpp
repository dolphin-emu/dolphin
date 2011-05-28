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

#include "BPMemory.h"
#include "FramebufferManager.h"
#include "GLUtil.h"
#include "Hash.h"
#include "HW/Memmap.h"
#include "ImageWrite.h"
#include "Render.h"
#include "TextureConverter.h"
#include "Tmem.h"
#include "VertexShaderManager.h"
#include "VirtualEFBCopy.h"

namespace OGL
{

bool SaveTexture(const char* filename, u32 textarget, u32 tex, int width, int height)
{
	std::vector<u32> data(width * height);
	glBindTexture(textarget, tex);
	glGetTexImage(textarget, 0, GL_BGRA, GL_UNSIGNED_BYTE, &data[0]);
	
	const GLenum err = GL_REPORT_ERROR();
	if (GL_NO_ERROR != err)
	{
		PanicAlert("Can't save texture, GL Error: %s", gluErrorString(err));
		return false;
	}

    return SaveTGA(filename, width, height, &data[0]);
}

TCacheEntry::TCacheEntry()
	: m_ramTexture(0)
{ }

TCacheEntry::~TCacheEntry()
{
	glDeleteTextures(1, &m_ramTexture);
	m_ramTexture = 0;
}

inline bool IsPaletted(u32 format)
{
	return format == GX_TF_C4 || format == GX_TF_C8 || format == GX_TF_C14X2;
}

// Compute the maximum number of mips a texture of given dims could have
// Some games (Luigi's Mansion for example) try to use too many mip levels.
static unsigned int ComputeMaxLevels(unsigned int width, unsigned int height)
{
	if (width == 0 || height == 0)
		return 0;
	return floorLog2( std::max(width, height) ) + 1;
}

void TCacheEntry::RefreshInternal(u32 ramAddr, u32 width, u32 height, u32 levels,
	u32 format, u32 tlutAddr, u32 tlutFormat, bool invalidated)
{
	m_loaded = 0;
	m_loadedDirty = false;
	m_loadedIsPaletted = false;
	m_bindMe = 0;

	// This is the earliest possible place to correct mip levels. Do so.
	unsigned int maxLevels = ComputeMaxLevels(width, height);
	levels = std::min(levels, maxLevels);

	Load(ramAddr, width, height, levels, format, tlutAddr, tlutFormat, invalidated);

	m_bindMe = m_loaded;
}

void TCacheEntry::Load(u32 ramAddr, u32 width, u32 height, u32 levels,
	u32 format, u32 tlutAddr, u32 tlutFormat, bool invalidated)
{
	bool refreshFromRam = true;

	// See if there is a virtual EFB copy available at this address
	VirtualEFBCopyMap& virtCopyMap = ((TextureCache*)g_textureCache)->GetVirtCopyMap();
	VirtualEFBCopyMap::iterator virtIt = virtCopyMap.find(ramAddr);

	if (virtIt != virtCopyMap.end())
	{
		if (g_ActiveConfig.bEFBCopyVirtualEnable)
		{
			// Try to load from the virtual copy instead of RAM.
			// FIXME: If copy fails to load, its memory region may have
			// been deallocated by the game. We ought to delete copies that aren't
			// being used.
			if (LoadFromVirtualCopy(ramAddr, width, height, levels, format, tlutAddr,
				tlutFormat, invalidated, (VirtualEFBCopy*)virtIt->second.get()))
				refreshFromRam = false;
		}
		else
		{
			// User must have turned off virtual copies mid-game. Delete any
			// that are found.
			// FIXME: Is it desirable to delete them as they are found instead
			// of just deleting them all at once?
			virtCopyMap.erase(virtIt);
		}
	}

	if (refreshFromRam)
		LoadFromRam(ramAddr, width, height, levels, format, tlutAddr, tlutFormat, invalidated);
}

void TCacheEntry::LoadFromRam(u32 ramAddr, u32 width, u32 height, u32 levels,
	u32 format, u32 tlutAddr, u32 tlutFormat, bool invalidated)
{
	int blockW = TexDecoder_GetBlockWidthInTexels(format);
	int blockH = TexDecoder_GetBlockHeightInTexels(format);

	const u8* src = Memory::GetPointer(ramAddr);
	const u16* tlut = (const u16*)&g_texMem[tlutAddr];
	
	bool dimsChanged = width != m_curWidth || height != m_curHeight || levels != m_curLevels;

	// Do we need to refresh the palette?
	u64 newPaletteHash = m_curPaletteHash;
	if (IsPaletted(format))
	{
		u32 paletteSize = TexDecoder_GetPaletteSize(format);
		newPaletteHash = GetHash64((const u8*)tlut, paletteSize, paletteSize);
		DEBUG_LOG(VIDEO, "Hash of tlut at 0x%.05X was taken... 0x%.016X", tlutAddr, newPaletteHash);
	}
		
	u64 newHash = m_curHash;

	// TODO: Use a shader to depalettize textures, like the DX11 plugin
	bool reloadTexture = !m_ramTexture || dimsChanged || format != m_curFormat ||
		newPaletteHash != m_curPaletteHash || tlutFormat != m_curTlutFormat;
	if (reloadTexture || invalidated)
	{
		// FIXME: Only the top-level mip is hashed.
		u32 sizeInBytes = TexDecoder_GetTextureSizeInBytes(width, height, format);
		newHash = GetHash64(src, sizeInBytes, sizeInBytes);
		reloadTexture |= (newHash != m_curHash);
		DEBUG_LOG(VIDEO, "Hash of texture at 0x%.08X was taken... 0x%.016X", ramAddr, newHash);
	}

	if (reloadTexture)
	{
		if (!m_ramTexture)
			glGenTextures(1, &m_ramTexture);

		glBindTexture(GL_TEXTURE_2D, m_ramTexture);
		
		// FIXME: Hash all the mips seperately?
		u32 mipWidth = width;
		u32 mipHeight = height;
		for (u32 level = 0; level < levels; ++level)
		{
			int actualWidth = (mipWidth + blockW-1) & ~(blockW-1);
			int actualHeight = (mipHeight + blockH-1) & ~(blockH-1);

			u8* decodeTemp = ((TextureCache*)g_textureCache)->GetDecodeTemp();
			PC_TexFormat pcFormat = TexDecoder_Decode(decodeTemp, src,
				actualWidth, actualHeight, format, tlut, tlutFormat, false);

			GLint useInternalFormat = 4;
			GLenum useFormat = GL_RGBA;
			GLenum useType = GL_UNSIGNED_BYTE;
			switch (pcFormat)
			{
			case PC_TEX_FMT_BGRA32:
				useInternalFormat = 4;
				useFormat = GL_BGRA;
				useType = GL_UNSIGNED_BYTE;
				break;
			case PC_TEX_FMT_RGBA32:
				useInternalFormat = 4;
				useFormat = GL_RGBA;
				useType = GL_UNSIGNED_BYTE;
				break;
			case PC_TEX_FMT_I4_AS_I8:
				useInternalFormat = GL_INTENSITY4;
				useFormat = GL_LUMINANCE;
				useType = GL_UNSIGNED_BYTE;
				break;
			case PC_TEX_FMT_IA4_AS_IA8:
				useInternalFormat = GL_LUMINANCE4_ALPHA4;
				useFormat = GL_LUMINANCE_ALPHA;
				useType = GL_UNSIGNED_BYTE;
				break;
			case PC_TEX_FMT_I8:
				useInternalFormat = GL_INTENSITY8;
				useFormat = GL_LUMINANCE;
				useType = GL_UNSIGNED_BYTE;
				break;
			case PC_TEX_FMT_IA8:
				useInternalFormat = GL_LUMINANCE8_ALPHA8;
				useFormat = GL_LUMINANCE_ALPHA;
				useType = GL_UNSIGNED_BYTE;
				break;
			case PC_TEX_FMT_RGB565:
				useInternalFormat = GL_RGB;
				useFormat = GL_RGB;
				useType = GL_UNSIGNED_SHORT_5_6_5;
				break;
			}

			if (actualWidth != mipWidth)
				glPixelStorei(GL_UNPACK_ROW_LENGTH, actualWidth);

			glTexImage2D(GL_TEXTURE_2D, level, useInternalFormat, mipWidth, mipHeight, 0,
				useFormat, useType, decodeTemp);

			if (actualWidth != mipWidth)
				glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

			src += TexDecoder_GetTextureSizeInBytes(mipWidth, mipHeight, format);
			
			mipWidth = (mipWidth > 1) ? mipWidth/2 : 1;
			mipHeight = (mipHeight > 1) ? mipHeight/2 : 1;
		}

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	m_curWidth = width;
	m_curHeight = height;
	m_curLevels = levels;
	m_curFormat = format;
	m_curHash = newHash;

	m_curPaletteHash = newPaletteHash;
	m_curTlutFormat = tlutFormat;

	m_loaded = m_ramTexture;
	m_loadedDirty = reloadTexture;
	// TODO: Use shader to depalettize RAM textures
	m_loadedIsPaletted = false;
}

bool TCacheEntry::LoadFromVirtualCopy(u32 ramAddr, u32 width, u32 height, u32 levels,
	u32 format, u32 tlutAddr, u32 tlutFormat, bool invalidated, VirtualEFBCopy* virt)
{
	u64 newHash = m_curHash;

	// If texture will be loaded to TMEM, make sure mem hash matches TCL hash.
	// Otherwise, it's acceptable to just use the fake texture as is.
	if (invalidated)
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

		if (newHash != virt->GetHash())
		{
			INFO_LOG(VIDEO, "EFB copy may have been modified since encoding; falling back to RAM");
			return false;
		}
	}

	GLuint virtualized = virt->Virtualize(ramAddr, width, height, levels,
		format, tlutAddr, tlutFormat, !g_ActiveConfig.bEFBCopyRAMEnable);
	if (!virtualized)
		return false;

	m_curWidth = width;
	m_curHeight = height;
	m_curLevels = levels;
	m_curFormat = format;
	m_curHash = newHash;

	m_loaded = virtualized;
	m_loadedDirty = virt->IsDirty();
	virt->ResetDirty();
	m_loadedIsPaletted = IsPaletted(format);
	return true;
}

TextureCache::TextureCache()
	: m_virtCopyFramebuf(0)
{
	glGenFramebuffersEXT(1, &m_virtCopyFramebuf);
}

TextureCache::~TextureCache()
{
	glDeleteFramebuffersEXT(1, &m_virtCopyFramebuf);
	m_virtCopyFramebuf = 0;
}

TCacheEntry* TextureCache::CreateEntry()
{
	return new TCacheEntry;
}

VirtualEFBCopy* TextureCache::CreateVirtualEFBCopy()
{
	return new VirtualEFBCopy;
}

u32 TextureCache::EncodeEFBToRAM(u8* dst, unsigned int dstFormat, unsigned int srcFormat,
	const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf)
{
	return TextureConverter::EncodeToRam(dst, dstFormat, srcFormat, srcRect, isIntensity, scaleByHalf);
}

}
