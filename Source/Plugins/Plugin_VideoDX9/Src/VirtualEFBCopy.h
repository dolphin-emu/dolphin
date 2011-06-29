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

#ifndef _VIDEODX9_VIRTUALEFBCOPY_H
#define _VIDEODX9_VIRTUALEFBCOPY_H

#include "VideoCommon.h"

#include "D3DUtil.h"
#include "TextureCacheBase.h"

namespace DX9
{

class VirtualCopyShaderManager
{

public:

	VirtualCopyShaderManager();
	~VirtualCopyShaderManager();

	LPDIRECT3DPIXELSHADER9 GetShader(bool scale, bool depth);

private:

	inline int MakeKey(bool scale, bool depth) {
		return (scale ? (1<<1) : 0) | (depth ? (1<<0) : 0);
	}

	LPDIRECT3DPIXELSHADER9 m_shaders[4];

};

class VirtualEFBCopy : public VirtualEFBCopyBase
{

public:

	VirtualEFBCopy();
	~VirtualEFBCopy();

	// When EFB is copied to RAM, this function is called to create and
	// refresh a virtual copy.
	void Update(u32 dstAddr, unsigned int dstFormat, unsigned int srcFormat,
		const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf);

	// When a virtual EFB copy is used as a texture, this function is called
	// to make a texture from it. Returns NULL if the formats are incompatible.
	LPDIRECT3DTEXTURE9 Virtualize(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat, bool force);
	
	unsigned int GetRealWidth() const { return m_realW; }
	unsigned int GetRealHeight() const { return m_realH; }
	
	bool IsDirty() const { return m_dirty; }
	void ResetDirty() { m_dirty = false; }

private:

	void VirtualizeShade(LPDIRECT3DTEXTURE9 texSrc, unsigned int srcFormat,
		bool scale, const EFBRectangle& srcRect,
		unsigned int virtualW, unsigned int virtualH,
		const float* colorMatrix, const float* colorAdd);
	
	// Properties of the "real" texture: width, height, hash of encoded data, etc.
	unsigned int m_realW;
	unsigned int m_realH;
	unsigned int m_virtualW;
	unsigned int m_virtualH;
	unsigned int m_dstFormat;

	LPDIRECT3DTEXTURE9 m_texture;
	bool m_dirty;

};

}

#endif // _VIDEODX9_VIRTUALEFBCOPY_H
