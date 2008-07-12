#include "D3DBase.h"
#include "D3DTexture.h"

namespace D3D
{
	LPDIRECT3DTEXTURE9 CreateTexture2D(const BYTE* buffer, const int width, const int height,const int pitch,  D3DFORMAT fmt)
	{
		DWORD *pBuffer = (DWORD *)buffer;
		LPDIRECT3DTEXTURE9 pTexture;

		// crazy bitmagic
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

		DWORD* pIn = pBuffer;
		switch(fmt) 
		{
		case D3DFMT_A8R8G8B8:
			{
				for (int y = 0; y < height; y++)
				{
					DWORD* pBits = (DWORD*)((BYTE*)Lock.pBits + (y * Lock.Pitch));
					memcpy(pBits,pIn, width * 4);
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

	void ReplaceTexture2D(LPDIRECT3DTEXTURE9 pTexture, const BYTE* buffer, const int width, const int height,const int pitch,  D3DFORMAT fmt)
	{
		DWORD *pBuffer = (DWORD *)buffer;
		int level=0;
		D3DLOCKED_RECT Lock;
		pTexture->LockRect(level, &Lock, NULL, 0 );
		DWORD* pIn = pBuffer;
		switch(fmt) 
		{
		case D3DFMT_A8R8G8B8:
			{
				for (int y = 0; y < height; y++)
				{
					DWORD* pBits = (DWORD*)((BYTE*)Lock.pBits + (y * Lock.Pitch));
					memcpy(pBits,pIn, width*4);
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
}
