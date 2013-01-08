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

#include <vector>
#include <cmath>


#include <fstream>
#ifdef _WIN32
#define _interlockedbittestandset workaround_ms_header_bug_platform_sdk6_set
#define _interlockedbittestandreset workaround_ms_header_bug_platform_sdk6_reset
#define _interlockedbittestandset64 workaround_ms_header_bug_platform_sdk6_set64
#define _interlockedbittestandreset64 workaround_ms_header_bug_platform_sdk6_reset64
#include <intrin.h>
#undef _interlockedbittestandset
#undef _interlockedbittestandreset
#undef _interlockedbittestandset64
#undef _interlockedbittestandreset64
#endif

#include "BPStructs.h"
#include "CommonPaths.h"
#include "FileUtil.h"
#include "FramebufferManager.h"
#include "Globals.h"
#include "Hash.h"
#include "HiresTextures.h"
#include "HW/Memmap.h"
#include "ImageWrite.h"
#include "MemoryUtil.h"
#include "PixelShaderCache.h"
#include "PixelShaderManager.h"
#include "Render.h"
#include "Statistics.h"
#include "StringUtil.h"
#include "TextureCache.h"
#include "TextureConverter.h"
#include "TextureDecoder.h"
#include "VertexShaderManager.h"
#include "VideoConfig.h"

namespace OGL
{

static u32 s_TempFramebuffer = 0;

static const GLint c_MinLinearFilter[8] = {
	GL_NEAREST,
	GL_NEAREST_MIPMAP_NEAREST,
	GL_NEAREST_MIPMAP_LINEAR,
	GL_NEAREST,
	GL_LINEAR,
	GL_LINEAR_MIPMAP_NEAREST,
	GL_LINEAR_MIPMAP_LINEAR,
	GL_LINEAR,
};

static const GLint c_WrapSettings[4] = {
	GL_CLAMP_TO_EDGE,
	GL_REPEAT,
	GL_MIRRORED_REPEAT,
	GL_REPEAT,
};

bool SaveTexture(const char* filename, u32 textarget, u32 tex, int virtual_width, int virtual_height, unsigned int level)
{
	int width = std::max(virtual_width >> level, 1);
	int height = std::max(virtual_height >> level, 1);
	std::vector<u32> data(width * height);
	glBindTexture(textarget, tex);
	glGetTexImage(textarget, level, GL_BGRA, GL_UNSIGNED_BYTE, &data[0]);
	
	const GLenum err = GL_REPORT_ERROR();
	if (GL_NO_ERROR != err)
	{
		PanicAlert("Can't save texture, GL Error: %s", gluErrorString(err));
		return false;
	}

    return SaveTGA(filename, width, height, &data[0]);
}

TextureCache::TCacheEntry::~TCacheEntry()
{
	if (texture)
	{
		glDeleteTextures(1, &texture);
		texture = 0;
	}
}

TextureCache::TCacheEntry::TCacheEntry()
{
	glGenTextures(1, &texture);
	GL_REPORT_ERRORD();
}

void TextureCache::TCacheEntry::Bind(unsigned int stage)
{
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texture);
	GL_REPORT_ERRORD();

	// TODO: is this already done somewhere else?
	TexMode0 &tm0 = bpmem.tex[stage >> 2].texMode0[stage & 3];
	TexMode1 &tm1 = bpmem.tex[stage >> 2].texMode1[stage & 3];
	SetTextureParameters(tm0, tm1);
}

bool TextureCache::TCacheEntry::Save(const char filename[], unsigned int level)
{
	// TODO: make ogl dump PNGs
	std::string tga_filename(filename);
	tga_filename.replace(tga_filename.size() - 3, 3, "tga");

	return SaveTexture(tga_filename.c_str(), GL_TEXTURE_2D, texture, virtual_width, virtual_height, level);
}

TextureCache::TCacheEntryBase* TextureCache::CreateTexture(unsigned int width,
	unsigned int height, unsigned int expanded_width,
	unsigned int tex_levels, PC_TexFormat pcfmt)
{
	int gl_format = 0,
		gl_iformat = 0,
		gl_type = 0;

	if (pcfmt != PC_TEX_FMT_DXT1)
	{
		switch (pcfmt)
		{
		default:
		case PC_TEX_FMT_NONE:
			PanicAlert("Invalid PC texture format %i", pcfmt); 
		case PC_TEX_FMT_BGRA32:
			gl_format = GL_BGRA;
			gl_iformat = 4;
			gl_type = GL_UNSIGNED_BYTE;
			break;

		case PC_TEX_FMT_RGBA32:
			gl_format = GL_RGBA;
			gl_iformat = 4;
			gl_type = GL_UNSIGNED_BYTE;
			break;

		case PC_TEX_FMT_I4_AS_I8:
			gl_format = GL_LUMINANCE;
			gl_iformat = GL_INTENSITY4;
			gl_type = GL_UNSIGNED_BYTE;
			break;

		case PC_TEX_FMT_IA4_AS_IA8:
			gl_format = GL_LUMINANCE_ALPHA;
			gl_iformat = GL_LUMINANCE4_ALPHA4;
			gl_type = GL_UNSIGNED_BYTE;
			break;

		case PC_TEX_FMT_I8:
			gl_format = GL_LUMINANCE;
			gl_iformat = GL_INTENSITY8;
			gl_type = GL_UNSIGNED_BYTE;
			break;

		case PC_TEX_FMT_IA8:
			gl_format = GL_LUMINANCE_ALPHA;
			gl_iformat = GL_LUMINANCE8_ALPHA8;
			gl_type = GL_UNSIGNED_BYTE;
			break;

		case PC_TEX_FMT_RGB565:
			gl_format = GL_RGB;
			gl_iformat = GL_RGB;
			gl_type = GL_UNSIGNED_SHORT_5_6_5;
			break;
		}
	}

	TCacheEntry &entry = *new TCacheEntry;
	entry.gl_format = gl_format;
	entry.gl_iformat = gl_iformat;
	entry.gl_type = gl_type;
	entry.pcfmt = pcfmt;

	entry.bHaveMipMaps = tex_levels != 1;

	return &entry;
}

void TextureCache::TCacheEntry::Load(unsigned int width, unsigned int height,
	unsigned int expanded_width, unsigned int level, bool autogen_mips)
{
	//glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texture);
	//GL_REPORT_ERRORD();

	if (pcfmt != PC_TEX_FMT_DXT1)
	{
	    if (expanded_width != width)
            glPixelStorei(GL_UNPACK_ROW_LENGTH, expanded_width);

		if (bHaveMipMaps && autogen_mips)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
			glTexImage2D(GL_TEXTURE_2D, level, gl_iformat, width, height, 0, gl_format, gl_type, temp);
			glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
		}
		else
		{
			glTexImage2D(GL_TEXTURE_2D, level, gl_iformat, width, height, 0, gl_format, gl_type, temp);
		}

		if (expanded_width != width)
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	}
	else
	{
		PanicAlert("PC_TEX_FMT_DXT1 support disabled");
		//glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
			//width, height, 0, expanded_width * expanded_height/2, temp);
	}
	GL_REPORT_ERRORD();
}

TextureCache::TCacheEntryBase* TextureCache::CreateRenderTargetTexture(
	unsigned int scaled_tex_w, unsigned int scaled_tex_h)
{
	TCacheEntry *const entry = new TCacheEntry;
	glBindTexture(GL_TEXTURE_2D, entry->texture);
	GL_REPORT_ERRORD();

	const GLenum
		gl_format = GL_RGBA,
		gl_iformat = 4,
		gl_type = GL_UNSIGNED_BYTE;

	glTexImage2D(GL_TEXTURE_2D, 0, gl_iformat, scaled_tex_w, scaled_tex_h, 0, gl_format, gl_type, NULL);
	
	GL_REPORT_ERRORD();

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	if (GL_REPORT_ERROR() != GL_NO_ERROR)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		GL_REPORT_ERRORD();
	}

	return entry;
}

void TextureCache::TCacheEntry::FromRenderTarget(u32 dstAddr, unsigned int dstFormat,
	unsigned int srcFormat, const EFBRectangle& srcRect,
	bool isIntensity, bool scaleByHalf, unsigned int cbufid,
	const float *colmat)
{
	glBindTexture(GL_TEXTURE_2D, texture);

	// Make sure to resolve anything we need to read from.
	const GLuint read_texture = (srcFormat == PIXELFMT_Z24) ?
		FramebufferManager::ResolveAndGetDepthTarget(srcRect) :
		FramebufferManager::ResolveAndGetRenderTarget(srcRect);

    GL_REPORT_ERRORD();

	if (type != TCET_EC_DYNAMIC || g_ActiveConfig.bCopyEFBToTexture)
	{
		if (s_TempFramebuffer == 0)
			glGenFramebuffersEXT(1, (GLuint*)&s_TempFramebuffer);

		FramebufferManager::SetFramebuffer(s_TempFramebuffer);
		// Bind texture to temporary framebuffer
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, texture, 0);
		GL_REPORT_FBO_ERROR();
		GL_REPORT_ERRORD();

		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glActiveTexture(GL_TEXTURE0);
		glEnable(GL_TEXTURE_RECTANGLE_ARB);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, read_texture);

		glViewport(0, 0, virtual_width, virtual_height);

		PixelShaderCache::SetCurrentShader((srcFormat == PIXELFMT_Z24) ? PixelShaderCache::GetDepthMatrixProgram() : PixelShaderCache::GetColorMatrixProgram());    
		PixelShaderManager::SetColorMatrix(colmat); // set transformation
		GL_REPORT_ERRORD();

		TargetRectangle targetSource = g_renderer->ConvertEFBRectangle(srcRect);

		glBegin(GL_QUADS);
		glTexCoord2f((GLfloat)targetSource.left,  (GLfloat)targetSource.bottom); glVertex2f(-1,  1);
		glTexCoord2f((GLfloat)targetSource.left,  (GLfloat)targetSource.top  ); glVertex2f(-1, -1);
		glTexCoord2f((GLfloat)targetSource.right, (GLfloat)targetSource.top  ); glVertex2f( 1, -1);
		glTexCoord2f((GLfloat)targetSource.right, (GLfloat)targetSource.bottom); glVertex2f( 1,  1);
		glEnd();

		GL_REPORT_ERRORD();

		// Unbind texture from temporary framebuffer
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, 0, 0);
	}

	if (false == g_ActiveConfig.bCopyEFBToTexture)
	{
		int encoded_size = TextureConverter::EncodeToRamFromTexture(
			addr,
			read_texture,
			srcFormat == PIXELFMT_Z24, 
			isIntensity, 
			dstFormat, 
			scaleByHalf, 
			srcRect);

		u8* dst = Memory::GetPointer(addr);
		u64 hash = GetHash64(dst,encoded_size,g_ActiveConfig.iSafeTextureCache_ColorSamples);

		// Mark texture entries in destination address range dynamic unless caching is enabled and the texture entry is up to date
		if (!g_ActiveConfig.bEFBCopyCacheEnable)
			TextureCache::MakeRangeDynamic(addr,encoded_size);
		else if (!TextureCache::Find(addr, hash))
			TextureCache::MakeRangeDynamic(addr,encoded_size);

		this->hash = hash;
	}

    FramebufferManager::SetFramebuffer(0);
    VertexShaderManager::SetViewportChanged();
    DisableStage(0);

    GL_REPORT_ERRORD();

    if (g_ActiveConfig.bDumpEFBTarget)
    {
		static int count = 0;
		SaveTexture(StringFromFormat("%sefb_frame_%i.tga", File::GetUserPath(D_DUMPTEXTURES_IDX).c_str(),
			count++).c_str(), GL_TEXTURE_2D, texture, virtual_width, virtual_height, 0);
    }
}

void TextureCache::TCacheEntry::SetTextureParameters(const TexMode0 &newmode, const TexMode1 &newmode1)
{
	// TODO: not used anywhere
	TexMode0 mode = newmode;
	//mode1 = newmode1;

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
		(newmode.mag_filter || g_Config.bForceFiltering) ? GL_LINEAR : GL_NEAREST);

	if (bHaveMipMaps) 
	{
		// TODO: not used anywhere
		if (g_ActiveConfig.bForceFiltering && newmode.min_filter < 4)
			mode.min_filter += 4; // take equivalent forced linear

		int filt = newmode.min_filter;            
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, c_MinLinearFilter[filt & 7]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, newmode1.min_lod >> 4);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, newmode1.max_lod >> 4);
		glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, (newmode.lod_bias / 32.0f));
	}
	else
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			(g_ActiveConfig.bForceFiltering || newmode.min_filter >= 4) ? GL_LINEAR : GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, c_WrapSettings[newmode.wrap_s]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, c_WrapSettings[newmode.wrap_t]);

	if (g_Config.iMaxAnisotropy >= 1)
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
			(float)(1 << g_ActiveConfig.iMaxAnisotropy));
}

TextureCache::~TextureCache()
{
    if (s_TempFramebuffer)
	{
        glDeleteFramebuffersEXT(1, (GLuint*)&s_TempFramebuffer);
        s_TempFramebuffer = 0;
    }
}

void TextureCache::DisableStage(unsigned int stage)
{
	glActiveTexture(GL_TEXTURE0 + stage);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_RECTANGLE_ARB);
}

}
