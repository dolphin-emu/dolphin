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

#ifndef _TELEVISION_H
#define _TELEVISION_H

#include "VideoCommon.h"

struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11PixelShader;

namespace DX11
{

class Television
{

public:

	Television();

	void Init();
	void Shutdown();

	// Submit video data to be drawn. This will change the current state of the
	// TV. xfbAddr points to YUYV data stored in GameCube/Wii RAM, but the XFB
	// may be virtualized when rendering so the RAM may not actually be read.
	void Submit(u32 xfbAddr, u32 width, u32 height);

	// Render the current state of the TV.
	void Render();

private:

	// Properties of last Submit call
	u32 m_curAddr;
	u32 m_curWidth;
	u32 m_curHeight;

	// Used for real XFB mode

	ID3D11Texture2D* m_yuyvTexture;
	ID3D11ShaderResourceView* m_yuyvTextureSRV;
	ID3D11PixelShader* m_pShader;

};

}

#endif
