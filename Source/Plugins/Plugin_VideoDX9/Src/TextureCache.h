#ifndef _TEXTURECACHE_H
#define _TEXTURECACHE_H


#include <map>

#include "D3DBase.h"

class TextureCache
{
	struct TCacheEntry
	{
		LPDIRECT3DTEXTURE9 texture;
		u32 addr;
		u32 hash;
		u32 paletteHash;
		u32 hashoffset;
		u32 oldpixel;
		bool isRenderTarget;
		bool isNonPow2;
		int frameCount;
		int w,h,fmt;
		TCacheEntry()
		{
			texture=0;
			isRenderTarget=0;
			hash=0;
		}
		void Destroy();
	};


	typedef std::map<DWORD,TCacheEntry> TexCache;

	static u8 *temp;
	static TexCache textures;

public:
	static void Init();
	static void Cleanup();
	static void Shutdown();
	static void Invalidate();
	static void Load(int stage, DWORD address, int width, int height, int format, int tlutaddr, int tlutfmt);
	static void CopyEFBToRenderTarget(DWORD address, RECT *source);
};

#endif