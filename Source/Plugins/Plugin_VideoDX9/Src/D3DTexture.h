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

#pragma once

#include "D3DBase.h"

namespace D3D
{
	LPDIRECT3DTEXTURE9 CreateTexture2D(const u8* buffer, const int width, const int height, const int pitch, D3DFORMAT fmt = D3DFMT_A8R8G8B8, bool swap_r_b = false);
	void ReplaceTexture2D(LPDIRECT3DTEXTURE9 pTexture, const u8* buffer, const int width, const int height, const int pitch, D3DFORMAT fmt, bool swap_r_b);
	LPDIRECT3DTEXTURE9 CreateRenderTarget(const int width, const int height);
	LPDIRECT3DSURFACE9 CreateDepthStencilSurface(const int width, const int height);
	LPDIRECT3DTEXTURE9 CreateOnlyTexture2D(const int width, const int height, D3DFORMAT fmt);
}
