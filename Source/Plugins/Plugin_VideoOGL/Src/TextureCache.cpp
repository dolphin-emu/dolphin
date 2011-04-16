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

#include "GLUtil.h"
#include "ImageWrite.h"
#include "FramebufferManager.h"
#include "VertexShaderManager.h"
#include "TextureConverter.h"
#include "BPMemory.h"
#include "Render.h"
#include "HW/Memmap.h"

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
	: m_inTmem(false), m_texture(0)
{
	glGenTextures(1, &m_texture);
}

TCacheEntry::~TCacheEntry()
{
	glDeleteTextures(1, &m_texture);
	m_texture = 0;
}

void TCacheEntry::EvictFromTmem()
{
	m_inTmem = false;
}

inline bool IsPaletted(u32 format)
{
	return format == GX_TF_C4 || format == GX_TF_C8 || format == GX_TF_C14X2;
}

void TCacheEntry::Refresh(u32 ramAddr, u32 width, u32 height, u32 levels,
	u32 format, u32 tlutAddr, u32 tlutFormat)
{
	int blockW = TexDecoder_GetBlockWidthInTexels(format);
	int blockH = TexDecoder_GetBlockHeightInTexels(format);

	const u8* src = Memory::GetPointer(ramAddr);
	const u16* tlut = (const u16*)&g_textureCache->GetCache()[tlutAddr];
	
	// Would real hardware reload data from RAM to TMEM?
	bool loadToTmem = !m_inTmem || ramAddr != m_ramAddr;
	
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
	bool reloadTexture = dimsChanged || format != m_curFormat ||
		newPaletteHash != m_curPaletteHash || tlutFormat != m_curTlutFormat;
	if (reloadTexture || loadToTmem)
	{
		// FIXME: Only the top-level mip is hashed.
		u32 sizeInBytes = TexDecoder_GetTextureSizeInBytes(width, height, format);
		newHash = GetHash64(src, sizeInBytes, sizeInBytes);
		reloadTexture |= (newHash != m_curHash);
		DEBUG_LOG(VIDEO, "Hash of texture at 0x%.08X was taken... 0x%.016X", ramAddr, newHash);
	}

	if (reloadTexture)
	{
		glBindTexture(GL_TEXTURE_2D, m_texture);
		
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

			mipWidth /= 2;
			mipHeight /= 2;
			++level;
		}

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	m_inTmem = true;
	m_ramAddr = ramAddr;

	m_curWidth = width;
	m_curHeight = height;
	m_curLevels = levels;
	m_curFormat = format;
	m_curHash = newHash;

	m_curPaletteHash = newPaletteHash;
	m_curTlutFormat = tlutFormat;
}

void TextureCache::EncodeEFB(u32 dstAddr, unsigned int dstFormat, unsigned int srcFormat,
	const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf)
{
	// TODO: Support virtual EFB copies

	g_renderer->ResetAPIState();

	if (g_ActiveConfig.bEFBCopyRAMEnable)
	{
		GLuint sourceTex = (srcFormat == PIXELFMT_Z24) ?
			FramebufferManager::ResolveAndGetDepthTarget(srcRect) :
			FramebufferManager::ResolveAndGetRenderTarget(srcRect);

		TextureConverter::EncodeToRamFromTexture(dstAddr, sourceTex,
			srcFormat == PIXELFMT_Z24, isIntensity, dstFormat, scaleByHalf,
			srcRect);

		GL_REPORT_ERRORD();
	}

	g_renderer->RestoreAPIState();

	// FIXME: These lines are necessary for the emulator to work, but why?
	FramebufferManager::SetFramebuffer(0);
	VertexShaderManager::SetViewportChanged();
	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_RECTANGLE_ARB);
}

TCacheEntry* TextureCache::CreateEntry()
{
	return new TCacheEntry;
}

}
