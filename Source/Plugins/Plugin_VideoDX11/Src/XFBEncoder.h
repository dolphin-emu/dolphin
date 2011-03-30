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

#ifndef _XFBENCODER_H
#define _XFBENCODER_H

#include "VideoCommon.h"

#include "D3DUtil.h"

namespace DX11
{

class XFBEncoder
{
public:
	XFBEncoder();
	~XFBEncoder();

	void Encode(u8* dst, u32 width, u32 height, const EFBRectangle& srcRect, float gamma);

private:
	SharedPtr<ID3D11Texture2D> m_out;
	ID3D11RenderTargetView* m_outRTV;
	SharedPtr<ID3D11Texture2D> m_outStage;
	SharedPtr<ID3D11Buffer> m_encodeParams;
	SharedPtr<ID3D11Buffer> m_quad;
	SharedPtr<ID3D11VertexShader> m_vShader;
	SharedPtr<ID3D11InputLayout> m_quadLayout;
	SharedPtr<ID3D11PixelShader> m_pShader;
	SharedPtr<ID3D11BlendState> m_xfbEncodeBlendState;
	ID3D11DepthStencilState* m_xfbEncodeDepthState;
	ID3D11RasterizerState* m_xfbEncodeRastState;
	ID3D11SamplerState* m_efbSampler;
};

}

#endif
