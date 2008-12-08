// Copyright (C) 2003-2008 Dolphin Project.

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

#include "D3DBase.h"

namespace D3D
{
	// Font creation flags
	#define D3DFONT_BOLD        0x0001
	#define D3DFONT_ITALIC      0x0002

	// Font rendering flags
	#define D3DFONT_CENTERED    0x0001

	//a cut-down variant of the DXSDK CD3DFont class
	class CD3DFont
	{
		LPDIRECT3DTEXTURE9      m_pTexture;   // The d3d texture for this font
		LPDIRECT3DVERTEXBUFFER9 m_pVB;        // VertexBuffer for rendering text
		//int     m_dwTexWidth;                 // Texture dimensions
		//int     m_dwTexHeight;
		float   m_fTextScale;
		float   m_fTexCoords[128-32][4];
		  
	public:
		CD3DFont();
		// 2D (no longer 3D) text drawing function
		// Initializing and destroying device-dependent objects
		void SetRenderStates();
		int Init();
		int Shutdown();
		int DrawTextScaled( float x, float y,
							float fXScale, float fYScale, 
							float spacing, u32 dwColor,
							const char* strText, bool center=true );
		  
		  
		// Constructor / destructor
		//~CD3DFont();
	};

	extern CD3DFont font;

	void quad2d(float x1, float y1, float x2, float y2, u32 color, float u1=0, float v1=0, float u2=1, float v2=1);
}
