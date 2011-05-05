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

#ifndef _VIDEODX9_TEXTURECACHE_H
#define _VIDEODX9_TEXTURECACHE_H

#include "TextureCacheBase.h"

#include "D3DUtil.h"

namespace DX9
{
	
class TCacheEntry : public TCacheEntryBase
{

public:

	TCacheEntry();
	~TCacheEntry();

	LPDIRECT3DTEXTURE9 GetTexture() { return m_texture; }

protected:

	void RefreshInternal(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat, bool invalidated);

private:

	void CreateRamTexture(u32 width, u32 height, u32 levels, D3DFORMAT d3dFormat);

	// Attributes of the currently-loaded texture
	u32 m_curWidth;
	u32 m_curHeight;
	u32 m_curLevels;
	u32 m_curFormat;
	u64 m_curHash;

	// Attributes of currently-loaded palette (if any)
	u64 m_curPaletteHash;
	u32 m_curTlutFormat;

	D3DFORMAT m_curD3DFormat;
	LPDIRECT3DTEXTURE9 m_texture;

};

class TextureCache : public TextureCacheBase
{

public:

	void EncodeEFB(u32 dstAddr, unsigned int dstFormat, unsigned int srcFormat,
		const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf);

	u8* GetDecodeTemp() { return m_decodeTemp; }

protected:

	TCacheEntry* CreateEntry();

private:

	GC_ALIGNED16(u8 m_decodeTemp[1024*1024*4]);

};

}

#endif // _VIDEODX9_TEXTURECACHE_H
