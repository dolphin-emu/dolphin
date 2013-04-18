// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _TEXTURECACHE_H
#define _TEXTURECACHE_H


#include <map>

#include "D3DBase.h"
#include "VideoCommon.h"
#include "BPMemory.h"

#include "TextureCacheBase.h"

namespace DX9
{

class TextureCache : public ::TextureCache
{
private:
	struct TCacheEntry : TCacheEntryBase
	{
		const LPDIRECT3DTEXTURE9 texture;

		D3DFORMAT d3d_fmt;
		bool swap_r_b;

		TCacheEntry(LPDIRECT3DTEXTURE9 _tex) : texture(_tex) {}
		~TCacheEntry();

		void Load(unsigned int width, unsigned int height,
			unsigned int expanded_width, unsigned int levels);

		void FromRenderTarget(u32 dstAddr, unsigned int dstFormat,
			unsigned int srcFormat, const EFBRectangle& srcRect,
			bool isIntensity, bool scaleByHalf, unsigned int cbufid,
			const float *colmat);

		void Bind(unsigned int stage);
		bool Save(const char filename[], unsigned int level);
	};

	TCacheEntryBase* CreateTexture(unsigned int width, unsigned int height,
		unsigned int expanded_width, unsigned int tex_levels, PC_TexFormat pcfmt);

	TCacheEntryBase* CreateRenderTargetTexture(unsigned int scaled_tex_w, unsigned int scaled_tex_h);
};

}

#endif
