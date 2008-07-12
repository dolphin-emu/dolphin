#pragma once

#include "D3DBase.h"

namespace D3D
{
	//////////////////////////////////////////////////////////////////////////
	//Simple box filter mipmap generator: should in theory be replaced with say a 
	//gaussian or something, but even this makes textures look great, especially
	//with anisotropic filtering enabled
	void FilterDown(DWORD *buffer, int w, int h, const int pitch);
	// __________________________________________________________________________________________________
	// calls filterdown which will trash _pBuffer as temp storage for mips
	LPDIRECT3DTEXTURE9 CreateTexture2D(const BYTE* buffer, const int width, const int height, const int pitch, D3DFORMAT fmt = D3DFMT_A8R8G8B8);
	void ReplaceTexture2D(LPDIRECT3DTEXTURE9 pTexture, const BYTE* buffer, const int width, const int height,const int pitch,  D3DFORMAT fmt);
	LPDIRECT3DTEXTURE9 CreateRenderTarget(const int width, const int height);
	LPDIRECT3DSURFACE9 CreateDepthStencilSurface(const int width, const int height);
}