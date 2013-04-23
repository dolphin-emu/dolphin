// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "D3DBase.h"

namespace DX9
{

namespace D3D
{
	LPDIRECT3DTEXTURE9 CreateTexture2D(const u8* buffer, const int width, const int height, const int pitch, D3DFORMAT fmt = D3DFMT_A8R8G8B8, bool swap_r_b = false, int levels = 1);
	void ReplaceTexture2D(LPDIRECT3DTEXTURE9 pTexture, const u8* buffer, const int width, const int height, const int pitch, D3DFORMAT fmt, bool swap_r_b, int level = 0);
	LPDIRECT3DTEXTURE9 CreateRenderTarget(const int width, const int height);
	LPDIRECT3DSURFACE9 CreateDepthStencilSurface(const int width, const int height);
	LPDIRECT3DTEXTURE9 CreateOnlyTexture2D(const int width, const int height, D3DFORMAT fmt);
}

}  //  namespace DX9
