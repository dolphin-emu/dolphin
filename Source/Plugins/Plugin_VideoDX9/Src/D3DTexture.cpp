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

#include "D3DBase.h"
#include "D3DTexture.h"

namespace D3D
{

LPDIRECT3DTEXTURE9 CreateTexture2D(const u8* buffer, const int width, const int height,const int pitch,  D3DFORMAT fmt)
{
	u32* pBuffer = (u32*)buffer;
	LPDIRECT3DTEXTURE9 pTexture;

	// crazy bitmagic, sorry :)
	bool isPow2 = !((width&(width-1)) || (height&(height-1)));

	HRESULT hr;
	// TODO(ector): allow mipmaps for non-pow textures on newer cards?
	if (!isPow2)
		hr = dev->CreateTexture(width, height, 1, 0, fmt, D3DPOOL_MANAGED, &pTexture, NULL);
	else
		hr = dev->CreateTexture(width, height, 0, D3DUSAGE_AUTOGENMIPMAP, fmt, D3DPOOL_MANAGED, &pTexture, NULL);

	if(FAILED(hr))
		return 0;

	int level = 0;

	D3DLOCKED_RECT Lock;
	pTexture->LockRect(level, &Lock, NULL, 0 );

	u32* pIn = pBuffer;
	switch(fmt) 
	{
	case D3DFMT_A8R8G8B8:
		{
			for (int y = 0; y < height; y++)
			{
				u32* pBits = (u32*)((u8*)Lock.pBits + (y * Lock.Pitch));
				memcpy(pBits, pIn, width * 4);
				pIn += pitch;
			}
		}
		break;
	case D3DFMT_DXT1:
		memcpy(Lock.pBits,buffer,(width/4)*(height/4)*8);
		break;
	}
	pTexture->UnlockRect(level); 
	return pTexture;
}

void ReplaceTexture2D(LPDIRECT3DTEXTURE9 pTexture, const u8* buffer, const int width, const int height,const int pitch,  D3DFORMAT fmt)
{
	u32* pBuffer = (u32*)buffer;
	int level = 0;
	D3DLOCKED_RECT Lock;
	pTexture->LockRect(level, &Lock, NULL, 0 );
	u32* pIn = pBuffer;
	switch(fmt) 
	{
	case D3DFMT_A8R8G8B8:
		{
			for (int y = 0; y < height; y++)
			{
				u32* pBits = (u32*)((u8*)Lock.pBits + (y * Lock.Pitch));
				memcpy(pBits, pIn, width * 4);
				pIn += pitch;
			}
		}
		break;
	case D3DFMT_DXT1:
		memcpy(Lock.pBits,buffer,(width/4)*(height/4)*8);
		break;
	}
	pTexture->UnlockRect(level); 
}

LPDIRECT3DTEXTURE9 CreateRenderTarget(const int width, const int height)
{
	LPDIRECT3DTEXTURE9 tex;
	HRESULT hr = dev->CreateTexture(width,height,0,D3DUSAGE_RENDERTARGET | D3DUSAGE_AUTOGENMIPMAP,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&tex,NULL);

	if (FAILED(hr))
		return 0;
	else
		return tex;
}

LPDIRECT3DSURFACE9 CreateDepthStencilSurface(const int width, const int height)
{
	LPDIRECT3DSURFACE9 surf;
	HRESULT hr = dev->CreateDepthStencilSurface(width,height,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,0,0,&surf,0);

	if (FAILED(hr))
		return 0;
	else
		return surf;
}

}  // namespace
