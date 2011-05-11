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
#include "VirtualEFBCopy.h"
#include "DepalettizeShader.h"

namespace DX9
{
	
class TCacheEntry : public TCacheEntryBase
{

public:

	TCacheEntry();
	~TCacheEntry();

	void TeardownDeviceObjects();

	LPDIRECT3DTEXTURE9 GetTexture() { return m_bindMe; }

protected:

	void RefreshInternal(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat, bool invalidated);

private:

	void Load(u32 ramAddr, u32 width, u32 height, u32 levels, u32 format,
		u32 tlutAddr, u32 tlutFormat, bool invalidated);

	void CreateRamTexture(u32 width, u32 height, u32 levels, D3DFORMAT d3dFormat);

	void LoadFromRam(u32 ramAddr, u32 width, u32 height, u32 levels, u32 format,
		u32 tlutAddr, u32 tlutFormat, bool invalidated);

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
	
	u32 m_curTlutFormat;
	u64 m_curPaletteHash; // Hash of palette data in TMEM

	D3DFORMAT m_curD3DFormat;
	LPDIRECT3DTEXTURE9 m_ramTexture;

	// Currently-loaded palette (if any)
	struct Palette
	{
		Palette()
			: tex(NULL)
		{ }

		LPDIRECT3DTEXTURE9 tex;
		D3DFORMAT d3dFormat;
		UINT numColors;
	} m_palette;

	// If loaded texture is paletted, this contains depalettized data.
	// Otherwise, this is not used.
	struct DepalStorage
	{
		DepalStorage()
			: tex(NULL)
		{ }

		LPDIRECT3DTEXTURE9 tex;
		UINT width;
		UINT height;
		// FIXME: Can paletted textures have more than one mip level?
	} m_depalStorage;

	LPDIRECT3DTEXTURE9 m_loaded;
	bool m_loadedDirty;
	bool m_loadedIsPaletted;
	LPDIRECT3DTEXTURE9 m_depalettized;

	LPDIRECT3DTEXTURE9 m_bindMe;

};

typedef std::map<u32, std::unique_ptr<VirtualEFBCopy> > VirtualEFBCopyMap;

class TextureCache : public TextureCacheBase
{

public:

	// Call before resetting the swap chain
	void TeardownDeviceObjects();

	void EncodeEFB(u32 dstAddr, unsigned int dstFormat, unsigned int srcFormat,
		const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf);

	VirtualEFBCopyMap& GetVirtCopyMap() { return m_virtCopyMap; }
	VirtualCopyShaderManager& GetVirtShaderManager() { return m_virtShaderManager; }
	DepalettizeShader& GetDepalShader() { return m_depalShader; }

	u8* GetDecodeTemp() { return m_decodeTemp; }

protected:

	TCacheEntry* CreateEntry();

private:

	VirtualEFBCopyMap m_virtCopyMap;
	VirtualCopyShaderManager m_virtShaderManager;
	DepalettizeShader m_depalShader;

	GC_ALIGNED16(u8 m_decodeTemp[1024*1024*4]);

};

}

#endif // _VIDEODX9_TEXTURECACHE_H
