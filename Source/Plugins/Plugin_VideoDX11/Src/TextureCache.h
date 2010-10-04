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

#include <map>

#include "D3DBase.h"
#include "D3DTexture.h"
#include "VideoCommon.h"
#include "BPMemory.h"

class TextureCache
{
public:
	struct TCacheEntry
	{
		D3DTexture2D* texture;

		u32 addr;
		u32 size_in_bytes;
		u64 hash;
		u32 paletteHash;
		u32 oldpixel;
		
		int frameCount;
		unsigned int w, h, fmt, MipLevels;
		int Realw, Realh, Scaledw, Scaledh;

		bool isRenderTarget;
		bool isNonPow2;

		TCacheEntry() : texture(NULL), addr(0), size_in_bytes(0), hash(0), paletteHash(0), oldpixel(0),
						frameCount(0), w(0), h(0), fmt(0), MipLevels(0), Realw(0), Realh(0), Scaledw(0), Scaledh(0),
						isRenderTarget(false), isNonPow2(true) {}

		void Destroy(bool shutdown);
		bool IntersectsMemoryRange(u32 range_address, u32 range_size);
	};

	static void Init();
	static void Cleanup();
	static void Shutdown();
	static void Invalidate(bool shutdown);
	static void InvalidateRange(u32 start_address, u32 size);
	static TCacheEntry* TextureCache::Load(unsigned int stage, u32 address, unsigned int width, unsigned int height, unsigned int tex_format, unsigned int tlutaddr, unsigned int tlutfmt, bool UseNativeMips, unsigned int maxlevel);
	static void CopyRenderTargetToTexture(u32 address, bool bFromZBuffer, bool bIsIntensityFmt, u32 copyfmt, unsigned int bScaleByHalf, const EFBRectangle &source_rect);

private:
	typedef std::map<u32,TCacheEntry> TexCache;

	static u8* temp;
	static TexCache textures;
};
