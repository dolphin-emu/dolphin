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

#ifndef _VIDEODX9_DEPALETTIZESHADER_H
#define _VIDEODX9_DEPALETTIZESHADER_H

#include "VideoCommon.h"

#include "D3DUtil.h"

namespace DX9
{

// "Depalletize" means to remove stuff from a wooden pallet.
// "Depalettize" means to convert from a color-indexed image to a direct-color
// image.
class DepalettizeShader
{

public:

	enum BaseType
	{
		Unorm4,
		Unorm8
	};

	DepalettizeShader();
	~DepalettizeShader();

	void Depalettize(LPDIRECT3DTEXTURE9 dstTex, LPDIRECT3DTEXTURE9 baseTex,
		BaseType baseType, LPDIRECT3DTEXTURE9 paletteTex);

private:

	LPDIRECT3DPIXELSHADER9 GetShader(BaseType type);

	// Depalettizing shader for 4-bit indices as normalized float
	LPDIRECT3DPIXELSHADER9 m_unorm4Shader;
	// Depalettizing shader for 8-bit indices as normalized float
	LPDIRECT3DPIXELSHADER9 m_unorm8Shader;

};

}

#endif
