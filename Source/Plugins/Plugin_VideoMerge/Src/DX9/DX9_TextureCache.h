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

#ifndef _TEXTURECACHE_H
#define _TEXTURECACHE_H


#include <map>

#include "DX9_D3DBase.h"
#include "VideoCommon.h"
#include "BPMemory.h"

#include "../TextureCache.h"

namespace DX9
{

class TextureCache : public TextureCacheBase
{
public:
	struct TCacheEntry : TCacheEntryBase
	{
		LPDIRECT3DTEXTURE9 texture;

		D3DFORMAT d3d_fmt;
		bool swap_r_b;
		bool isDynamic;	// mofified from cpu

		TCacheEntry(LPDIRECT3DTEXTURE9 _texture);
		~TCacheEntry();

		void Load(unsigned int width, unsigned int height,
			unsigned int expanded_width, unsigned int levels);

		void FromRenderTarget(bool bFromZBuffer, bool bScaleByHalf,
			unsigned int cbufid, const float colmat[], const EFBRectangle &source_rect);

		void Bind(unsigned int stage);
	};

public:

	TCacheEntryBase* CreateTexture(unsigned int width, unsigned int height,
		unsigned int expanded_width, unsigned int tex_levels, PC_TexFormat pcfmt);

	TCacheEntryBase* CreateRenderTargetTexture(int scaled_tex_w, int scaled_tex_h);

};

}

#endif
