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

namespace DX11
{

class D3DTexture2D;
class TextureEncoder;
class TexCopyLookaside;
	
class TCacheEntry : public TCacheEntryBase
{

public:

	TCacheEntry();

	void EvictFromTmem();
	void Refresh(u32 ramAddr, u32 width, u32 height, u32 levels, u32 format, u32 tlutAddr, u32 tlutFormat);
	void Bind(int stage);

private:

	void Load(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat);

	void LoadFromRam(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat);

	void CreateRamTexture(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat);

	void ReloadRamTexture(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat);

	bool LoadFromTcl(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat, TexCopyLookaside* tcl);

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

	bool m_inTmem;
	u32 m_ramAddr;

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
	SharedPtr<ID3D11Texture1D> m_palette;
	SharedPtr<ID3D11ShaderResourceView> m_paletteSRV;
	
	bool m_fromTcl;
	D3DTexture2D* m_loaded;
	bool m_loadedDirty;
	D3DTexture2D* m_depalettized;
	D3DTexture2D* m_bindMe;

};

typedef std::map<u32, std::unique_ptr<TexCopyLookaside> > TexCopyLookasideMap;

class TextureCache : public TextureCacheBase
{

public:

	TextureCache();

	void EncodeEFB(u32 dstAddr, unsigned int dstFormat, unsigned int srcFormat,
		const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf);

	u8* GetDecodeTemp() { return m_decodeTemp; }
	TexCopyLookasideMap& GetTclMap() { return m_tclMap; }
	
	SharedPtr<ID3D11PixelShader> GetDepal4Shader();
	SharedPtr<ID3D11PixelShader> GetDepal8Shader();
	SharedPtr<ID3D11PixelShader> GetDepalUintShader();

protected:

	TCacheEntry* CreateEntry();

private:

	u8 m_decodeTemp[1024*1024*4];

	// FIXME: Should the EFB encoder be embedded in the texture cache class?
	std::unique_ptr<TextureEncoder> m_encoder;
	TexCopyLookasideMap m_tclMap;

	// Depalettizing shader for 4-bit indices as normalized float
	SharedPtr<ID3D11PixelShader> m_depal4Shader;
	// Depalettizing shader for 8-bit indices as normalized float
	SharedPtr<ID3D11PixelShader> m_depal8Shader;
	// Depalettizing shader for indices as uint
	SharedPtr<ID3D11PixelShader> m_depalUintShader;

};

}

#endif
