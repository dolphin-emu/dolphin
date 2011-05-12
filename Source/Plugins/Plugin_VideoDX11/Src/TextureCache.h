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

#ifndef _VIDEODX11_TEXTURECACHE_H
#define _VIDEODX11_TEXTURECACHE_H

#include "TextureCacheBase.h"

#include "D3DUtil.h"
#include "DepalettizeShader.h"
#include "VirtualEFBCopy.h"

namespace DX11
{

class D3DTexture2D;
class TextureEncoder;
class TexCopyLookaside;
	
class TCacheEntry : public TCacheEntryBase
{

public:

	TCacheEntry();

	D3DTexture2D* GetTexture() { return m_bindMe; }
	
protected:

	void RefreshInternal(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat, bool invalidated);

private:

	void Load(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat, bool invalidated);

	void LoadFromRam(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat, bool invalidated);

	void CreateRamTexture(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat);

	void ReloadRamTexture(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat);

	bool LoadFromVirtualCopy(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat, bool invalidated, VirtualEFBCopy* tcl);

	void Depalettize(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat);

	// Returns true if TLUT changed, false if not
	bool RefreshTlut(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat);

	void CreatePaletteTexture(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat);

	// Run the depalettizing shader
	void DepalettizeShader(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat);

	// Attributes of the currently-loaded texture
	u32 m_curWidth;
	u32 m_curHeight;
	u32 m_curLevels;
	u32 m_curFormat;
	u64 m_curHash;

	// Attributes of currently-loaded palette (if any)
	u64 m_curPaletteHash;
	u32 m_curTlutFormat;
	u32 m_lastPalettedFormat;
	
	// If format is paletted, this texture contains palette indices.
	// If format is not paletted, this texture contains bindable data.
	// If entry is from TCL, this texture is not used.
	std::unique_ptr<D3DTexture2D> m_ramTexture;

	// If loaded texture is paletted, this contains depalettized data.
	// Otherwise, this is not used.
	struct
	{
		std::unique_ptr<D3DTexture2D> tex;
		UINT width;
		UINT height;
		UINT levels;
	} m_depalStorage;

	// If format is paletted, this contains the palette's RGBA data.
	SharedPtr<ID3D11Buffer> m_palette;
	SharedPtr<ID3D11ShaderResourceView> m_paletteSRV;
	
	bool m_fromVirtCopy;
	D3DTexture2D* m_loaded;
	bool m_loadedDirty;
	D3DTexture2D* m_depalettized;
	D3DTexture2D* m_bindMe;

};

typedef std::map<u32, std::unique_ptr<VirtualEFBCopy> > VirtualEFBCopyMap;

class TextureCache : public TextureCacheBase
{

public:

	TextureCache();

	void EncodeEFB(u32 dstAddr, unsigned int dstFormat, unsigned int srcFormat,
		const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf);

	u8* GetDecodeTemp() { return m_decodeTemp; }
	VirtualEFBCopyMap& GetVirtCopyMap() { return m_virtCopyMap; }
	
	VirtualCopyShaderManager& GetVirtShaderManager() { return m_virtShaderManager; }
	DepalettizeShader& GetDepalShader() { return m_depalShader; }

protected:

	TCacheEntry* CreateEntry();

private:

	// FIXME: Should the EFB encoder be embedded in the texture cache class?
	std::unique_ptr<TextureEncoder> m_encoder;
	VirtualEFBCopyMap m_virtCopyMap;

	VirtualCopyShaderManager m_virtShaderManager;
	DepalettizeShader m_depalShader;

	GC_ALIGNED16(u8 m_decodeTemp[1024*1024*4]);

};

}

#endif
