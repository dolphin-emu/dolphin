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

#ifndef _OGL_TEXTURECACHE_H_
#define _OGL_TEXTURECACHE_H_

#include "VideoCommon.h"
#include "BPMemory.h"

#include "OGL_GLUtil.h"

#include "../TextureCache.h"

namespace OGL
{

class TextureCache : public ::TextureCacheBase
{
public:
	struct TCacheEntry : TCacheEntryBase
	{
	    GLuint texture;

		bool isDynamic;
		bool bHaveMipMaps;

		PC_TexFormat pcfmt;

		int gl_format;
		int gl_iformat;
		int gl_type;

        TexMode0 mode; // current filter and clamp modes that texture is set to
		TexMode1 mode1; // current filter and clamp modes that texture is set to

		void SetTextureParameters(TexMode0 &newmode, TexMode1 &newmode1);

		TCacheEntry();
		~TCacheEntry();

		void Load(unsigned int width, unsigned int height,
			unsigned int expanded_width, unsigned int level);

		void FromRenderTarget(bool bFromZBuffer, bool bScaleByHalf,
			unsigned int cbufid, const float colmat[], const EFBRectangle &source_rect);

		void Bind(unsigned int stage);
	};

	~TextureCache();

	TCacheEntryBase* CreateTexture(unsigned int width, unsigned int height,
		unsigned int expanded_width, unsigned int tex_levels, PC_TexFormat pcfmt);

	TCacheEntryBase* CreateRenderTargetTexture(int scaled_tex_w, int scaled_tex_h);

    void CopyRenderTargetToTexture(u32 address, bool bFromZBuffer, bool bIsIntensityFmt, u32 copyfmt, unsigned int bScaleByHalf, const EFBRectangle &source);

	static void DisableStage(int stage); // sets active texture
};

//bool SaveTexture(const char* filename, u32 textarget, u32 tex, int width, int height);

}

#endif // _TEXTUREMNGR_H_
