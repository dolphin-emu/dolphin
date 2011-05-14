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

#ifndef _PSTEXTUREENCODER_H
#define _PSTEXTUREENCODER_H

#include "TextureEncoder.h"
#include "D3DUtil.h"

namespace DX11
{

class PSTextureEncoder : public TextureEncoder
{
public:
	PSTextureEncoder();
	~PSTextureEncoder();

	size_t Encode(u8* dst, unsigned int dstFormat, D3DTexture2D* srcTex,
		unsigned int srcFormat, const EFBRectangle& srcRect, bool isIntensity,
		bool scaleByHalf);

private:
	bool m_ready;

	SharedPtr<ID3D11Texture2D> m_out;
	ID3D11RenderTargetView* m_outRTV;
	SharedPtr<ID3D11Texture2D> m_outStage;
	SharedPtr<ID3D11Buffer> m_encodeParams;

	bool InitStaticMode();
	bool SetStaticShader(unsigned int dstFormat, unsigned int srcFormat,
		bool isIntensity, bool scaleByHalf);

	typedef unsigned int ComboKey; // Key for a shader combination

	ComboKey MakeComboKey(unsigned int dstFormat, bool isDepth,
		bool scaleByHalf)
	{
		return (dstFormat << 2) | (isDepth ? (1<<1) : 0)
			| (scaleByHalf ? (1<<0) : 0);
	}

	typedef std::map<ComboKey, SharedPtr<ID3D11PixelShader> > ComboMap;

	ComboMap m_staticShaders;

	SharedPtr<ID3D11PixelShader> m_useThisPS;
};

}

#endif
