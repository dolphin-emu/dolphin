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

#ifndef _VIDEOOGL_TEXTURECACHE_H
#define _VIDEOOGL_TEXTURECACHE_H

#include "TextureCacheBase.h"

#include "GLUtil.h"
#include "Depalettizer.h"
#include "VirtualEFBCopy.h"

namespace OGL
{

bool SaveTexture(const char* filename, u32 textarget, u32 tex, int width, int height);

class TCacheEntry : public TCacheEntryBase
{

public:

	GLuint GetTexture() { return m_bindMe; }

protected:

	void RefreshInternal(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat, bool invalidated);

private:

	void Load(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat, bool invalidated);

	void LoadFromRam(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat, bool invalidated);

	bool LoadFromVirtualCopy(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat, bool invalidated, VirtualEFBCopy* virt);

	void Depalettize(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat);

	// Returns true if palette is dirty
	bool RefreshPalette(u32 format, u32 tlutAddr, u32 tlutFormat);

	// Attributes of the currently-loaded texture
	u32 m_curWidth;
	u32 m_curHeight;
	u32 m_curLevels;
	u32 m_curFormat;
	u64 m_curHash;

	// Attributes of currently-loaded palette (if any)
	u32 m_curTlutFormat;
	u64 m_curPaletteHash;

	// If loaded texture comes from RAM, this holds it.
	struct RamStorage
	{
		RamStorage()
			: tex(0)
		{ }
		~RamStorage();

		GLuint tex;
	} m_ramStorage;

	// Currently-loaded palette (if any)
	struct Palette
	{
		Palette()
			: tex(0)
		{ }
		~Palette();

		GLuint tex;
	} m_palette;

	// If loaded texture is paletted, this contains depalettized data.
	// Otherwise, this is not used.
	struct DepalStorage
	{
		DepalStorage()
			: tex(0)
		{ }
		~DepalStorage();

		GLuint tex;
		u32 width;
		u32 height;
	} m_depalStorage;

	GLuint m_loaded;
	bool m_loadedDirty;
	bool m_loadedIsPaletted;
	u32 m_loadedWidth;
	u32 m_loadedHeight;
	GLuint m_depalettized;

	GLuint m_bindMe;

};

class TextureCache : public TextureCacheBase
{

public:

	TextureCache();
	~TextureCache();

	void EncodeEFB(u32 dstAddr, unsigned int dstFormat, unsigned int srcFormat,
		const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf);

	Depalettizer& GetDepalettizer() { return m_depal; }
	VirtualEFBCopyMap& GetVirtCopyMap() { return m_virtCopyMap; }
	GLuint GetVirtCopyFramebuf() { return m_virtCopyFramebuf; }
	u8* GetDecodeTemp() { return m_decodeTemp; }

protected:

	TCacheEntry* CreateEntry();
	VirtualEFBCopy* CreateVirtualEFBCopy();

	u32 EncodeEFBToRAM(u8* dst, unsigned int dstFormat, unsigned int srcFormat,
		const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf);

private:

	Depalettizer m_depal;
	GLuint m_virtCopyFramebuf;

	GC_ALIGNED16(u8 m_decodeTemp[1024*1024*4]);

};

}

#endif // _VIDEOOGL_TEXTURECACHE_H
