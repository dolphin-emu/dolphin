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
		void Destroy(bool shutdown);
	};


	typedef std::map<u32,TCacheEntry> TexCache;

	static u8 *temp;
	static TexCache textures;

public:
	static void Init();
	static void Cleanup();
	static void Shutdown();
	static void Invalidate(bool shutdown);
	static void Load(int stage, u32 address, int width, int height, int format, int tlutaddr, int tlutfmt);
	static void CopyEFBToRenderTarget(u32 address, RECT *source);
};

#endif