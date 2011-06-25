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

#ifndef _VIDEODX11_DEPALETTIZESHADER_H
#define _VIDEODX11_DEPALLETIZESHADER_H

#include "VideoCommon.h"
#include "D3DUtil.h"

namespace DX11
{

class D3DTexture2D;

class DepalettizeShader
{

public:

	enum BaseType
	{
		Uint,
		Unorm4,
		Unorm8
	};

	DepalettizeShader();
	~DepalettizeShader();

	// Depalettize from baseTex into dstTex using paletteSRV.
	// dstTex should be a render-target with the same dimensions as baseTex.
	// paletteSRV should be a view of an R8G8B8A8_UNORM buffer containing the
	// RGBA values of the palette.
	void Depalettize(D3DTexture2D* dstTex, D3DTexture2D* baseTex,
		BaseType baseType, ID3D11ShaderResourceView* paletteSRV);

private:

	ID3D11PixelShader* GetShader(BaseType baseType);
	
	// Depalettizing shader for uint indices
	ID3D11PixelShader* m_uintShader;
	// Depalettizing shader for 4-bit indices as normalized float
	ID3D11PixelShader* m_unorm4Shader;
	// Depalettizing shader for 8-bit indices as normalized float
	ID3D11PixelShader* m_unorm8Shader;

};

}

#endif
