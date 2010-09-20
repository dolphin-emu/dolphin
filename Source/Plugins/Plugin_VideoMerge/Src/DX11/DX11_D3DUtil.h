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

#pragma	once

#include <math.h>
#include <MathUtil.h>

#include "DX11_D3DBase.h"

namespace DX11
{

namespace D3D
{
	// Font creation flags
	#define D3DFONT_BOLD        0x0001
	#define D3DFONT_ITALIC      0x0002

	// Font rendering flags
	#define D3DFONT_CENTERED    0x0001

	class CD3DFont
	{
		ID3D11ShaderResourceView* m_pTexture;
		ID3D11Buffer* m_pVB;
		ID3D11InputLayout* m_InputLayout;
		ID3D11PixelShader* m_pshader;
		ID3D11VertexShader* m_vshader;
		ID3D11BlendState* m_blendstate;
		ID3D11RasterizerState* m_raststate;
		const int m_dwTexWidth;
		const int m_dwTexHeight;
		unsigned int m_LineHeight;
		float m_fTexCoords[128-32][4];

	public:
		CD3DFont();
		// 2D text drawing function
		// Initializing and destroying device-dependent objects
		int Init();
		int Shutdown();
		int DrawTextScaled(float x, float y,
							float size,
							float spacing, u32 dwColor,
							const char* strText, bool center=true);
	};

	extern CD3DFont font;

	void InitUtils();
	void ShutdownUtils();

	void SetPointCopySampler();
	void SetLinearCopySampler();

	void drawShadedTexQuad(ID3D11ShaderResourceView* texture,
						const D3D11_RECT* rSource,
						int SourceWidth,
						int SourceHeight,
						ID3D11PixelShader* PShader,
						ID3D11VertexShader* VShader,
						ID3D11InputLayout* layout);
	void drawShadedTexSubQuad(ID3D11ShaderResourceView* texture,
							const MathUtil::Rectangle<float>* rSource,
							int SourceWidth,
							int SourceHeight,
							const MathUtil::Rectangle<float>* rDest,
							ID3D11PixelShader* PShader,
							ID3D11VertexShader* Vshader,
							ID3D11InputLayout* layout);
	void drawClearQuad(u32 Color, float z, ID3D11PixelShader* PShader, ID3D11VertexShader* Vshader, ID3D11InputLayout* layout);
	void SaveRenderStates();
	void RestoreRenderStates();
}

}
