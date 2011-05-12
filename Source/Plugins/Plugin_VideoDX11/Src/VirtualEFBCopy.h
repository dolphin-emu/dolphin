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

#ifndef _VIDEODX11_VIRTUALEFBCOPY_H
#define _VIDEODX11_VIRTUALEFBCOPY_H

#include "VideoCommon.h"
#include "D3DBase.h"

namespace DX11
{

class VirtualCopyShaderManager
{

public:

	SharedPtr<ID3D11PixelShader> GetShader(bool scale, bool depth);
	SharedPtr<ID3D11Buffer> GetParams() { return m_shaderParams; }

private:
	
	inline int MakeKey(bool scale, bool depth) {
		return (scale ? (1<<1) : 0) | (depth ? (1<<0) : 0);
	}

	SharedPtr<ID3D11PixelShader> m_shaders[4];
	SharedPtr<ID3D11Buffer> m_shaderParams;

};

class VirtualEFBCopy
{

public:

	VirtualEFBCopy();

	void Update(u32 dstAddr, unsigned int dstFormat, unsigned int srcFormat,
		const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf);

	// Returns NULL if texture could not be faked (caller should decode from
	// RAM instead)
	D3DTexture2D* FakeTexture(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat);

	unsigned int GetRealWidth() const { return m_realW; }
	unsigned int GetRealHeight() const { return m_realH; }

	u64 GetHash() const { return m_hash; }
	void SetHash(u64 hash) { m_hash = hash; }

	bool IsDirty() const { return m_dirty; }
	void ResetDirty() { m_dirty = false; }

private:

	void FakeEncodeShade(D3DTexture2D* srcTex, unsigned int srcFormat,
		bool yuva, bool scale,
		unsigned int posX, unsigned int posY,
		unsigned int virtualW, unsigned int virtualH,
		const float* colorMatrix, const float* colorAdd);

	// Properties of the "real" texture: width, height, hash of encoded data, etc.
	unsigned int m_realW;
	unsigned int m_realH;
	unsigned int m_virtualW;
	unsigned int m_virtualH;
	unsigned int m_dstFormat;

	// This is not maintained by VirtualEFBCopy. It must be handled externally.
	u64 m_hash;

	// Fake base: Created and updated at the time of EFB copy
	std::unique_ptr<D3DTexture2D> m_fakeBase;
	bool m_dirty;

};

}

#endif
