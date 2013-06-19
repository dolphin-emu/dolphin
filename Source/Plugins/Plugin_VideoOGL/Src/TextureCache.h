// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
	TextureCache();
	static void DisableStage(unsigned int stage);
	static void SetStage();
	static void SetNextStage(unsigned int stage);

private:
	struct TCacheEntry : TCacheEntryBase
	{
		GLuint texture;
		GLuint framebuffer;

		PC_TexFormat pcfmt;

		int gl_format;
		int gl_iformat;
		int gl_type;

		int m_tex_levels;

		//TexMode0 mode; // current filter and clamp modes that texture is set to
		//TexMode1 mode1; // current filter and clamp modes that texture is set to

		TCacheEntry();
		~TCacheEntry();

		void Load(unsigned int width, unsigned int height,
			unsigned int expanded_width, unsigned int level);

		void FromRenderTarget(u32 dstAddr, unsigned int dstFormat,
			unsigned int srcFormat, const EFBRectangle& srcRect,
			bool isIntensity, bool scaleByHalf, unsigned int cbufid,
			const float *colmat);

		void Bind(unsigned int stage);
		bool Save(const char filename[], unsigned int level);
	};

	~TextureCache();

	TCacheEntryBase* CreateTexture(unsigned int width, unsigned int height,
		unsigned int expanded_width, unsigned int tex_levels, PC_TexFormat pcfmt);

	TCacheEntryBase* CreateRenderTargetTexture(unsigned int scaled_tex_w, unsigned int scaled_tex_h);
};

bool SaveTexture(const char* filename, u32 textarget, u32 tex, int virtual_width, int virtual_height, unsigned int level);

}

#endif // _TEXTUREMNGR_H_
