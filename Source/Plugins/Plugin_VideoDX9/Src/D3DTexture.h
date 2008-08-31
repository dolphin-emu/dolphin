#pragma once

#include "D3DBase.h"

namespace D3D
{
	LPDIRECT3DTEXTURE9 CreateTexture2D(const u8* buffer, const int width, const int height, const int pitch, D3DFORMAT fmt = D3DFMT_A8R8G8B8);
	void ReplaceTexture2D(LPDIRECT3DTEXTURE9 pTexture, const u8* buffer, const int width, const int height,const int pitch,  D3DFORMAT fmt);
	LPDIRECT3DTEXTURE9 CreateRenderTarget(const int width, const int height);
	LPDIRECT3DSURFACE9 CreateDepthStencilSurface(const int width, const int height);
}
