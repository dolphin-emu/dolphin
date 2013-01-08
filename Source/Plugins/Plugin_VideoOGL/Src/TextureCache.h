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

#ifndef _TEXTUREMNGR_H_
#define _TEXTUREMNGR_H_

#include <map>

#include "VideoCommon.h"
#include "GLUtil.h"
#include "BPStructs.h"

#include "TextureCacheBase.h"

namespace OGL
{

class TextureCache : public ::TextureCache
{
public:
	static void DisableStage(unsigned int stage);

private:
	struct TCacheEntry : TCacheEntryBase
	{
		GLuint texture;

		PC_TexFormat pcfmt;

		int gl_format;
		int gl_iformat;
		int gl_type;

		bool bHaveMipMaps;

		//TexMode0 mode; // current filter and clamp modes that texture is set to
		//TexMode1 mode1; // current filter and clamp modes that texture is set to

		TCacheEntry();
		~TCacheEntry();

		void Load(unsigned int width, unsigned int height,
			unsigned int expanded_width, unsigned int level, bool autogen_mips = false);

		void FromRenderTarget(u32 dstAddr, unsigned int dstFormat,
			unsigned int srcFormat, const EFBRectangle& srcRect,
			bool isIntensity, bool scaleByHalf, unsigned int cbufid,
			const float *colmat);

		void Bind(unsigned int stage);
		bool Save(const char filename[], unsigned int level);

	private:
		void SetTextureParameters(const TexMode0 &newmode, const TexMode1 &newmode1);
	};

	~TextureCache();

	TCacheEntryBase* CreateTexture(unsigned int width, unsigned int height,
		unsigned int expanded_width, unsigned int tex_levels, PC_TexFormat pcfmt);

	TCacheEntryBase* CreateRenderTargetTexture(unsigned int scaled_tex_w, unsigned int scaled_tex_h);
};

bool SaveTexture(const char* filename, u32 textarget, u32 tex, int virtual_width, int virtual_height, unsigned int level);

}

#endif // _TEXTUREMNGR_H_
