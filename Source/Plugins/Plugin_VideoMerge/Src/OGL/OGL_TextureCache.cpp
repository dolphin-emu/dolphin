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

// Common
#include "CommonPaths.h"
#include "StringUtil.h"
#include "MemoryUtil.h"
#include "FileUtil.h"

// VideoCommon
#include "VideoConfig.h"
#include "Hash.h"
#include "Statistics.h"
#include "Profiler.h"
#include "ImageWrite.h"
#include "BPStructs.h"
#include "TextureDecoder.h"
#include "PixelShaderManager.h"
#include "HiresTextures.h"
#include "VertexShaderManager.h"

// OGL
#include "OGL_TextureCache.h"
#include "OGL_PixelShaderCache.h"
#include "OGL_TextureConverter.h"
#include "OGL_FramebufferManager.h"

#include "../Main.h"

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

void TextureCache::TCacheEntry::Load(unsigned int width, unsigned int height,
	unsigned int expanded_width, unsigned int level)
{
	if (bHaveMipMaps)
	{			
		if (pcfmt != PC_TEX_FMT_DXT1)
		{
			if (expanded_width != (int)width)
				glPixelStorei(GL_UNPACK_ROW_LENGTH, expanded_width);

			glTexImage2D(GL_TEXTURE_2D, level, gl_iformat, width, height, 0, gl_format, gl_type, TextureCache::temp);	

			if (expanded_width != (int)width)
				glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		}
		else
		{
			// TODO: 
			//glCompressedTexImage2D(target, level, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,	width, height, 0, expanded_width*expanded_height/2, temp);				
		}
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
		glTexImage2D(GL_TEXTURE_2D, 0, gl_iformat, width, height, 0, gl_format, gl_type, TextureCache::temp);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
	}
}

void TextureCache::TCacheEntry::FromRenderTarget(bool bFromZBuffer,	bool bScaleByHalf,
	unsigned int cbufid, const float colmat[], const EFBRectangle &source_rect)
{
	glBindTexture(GL_TEXTURE_2D, texture);

	// Make sure to resolve anything we need to read from.
	const GLuint read_texture = bFromZBuffer ? FramebufferManager::ResolveAndGetDepthTarget(source_rect) : FramebufferManager::ResolveAndGetRenderTarget(source_rect);

    GL_REPORT_ERRORD();

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
   
	glViewport(0, 0, Scaledw, Scaledh);

	PixelShaderCache::SetCurrentShader(bFromZBuffer ? PixelShaderCache::GetDepthMatrixProgram() : PixelShaderCache::GetColorMatrixProgram());
	const float* const fConstAdd = colmat + 16;		// fConstAdd is the last 4 floats of colmat
	PixelShaderManager::SetColorMatrix(colmat, fConstAdd); // set transformation

	GL_REPORT_ERRORD();

	TargetRectangle targetSource = g_renderer->ConvertEFBRectangle(source_rect);

	glBegin(GL_QUADS);
	glTexCoord2f((GLfloat)targetSource.left,  (GLfloat)targetSource.bottom); glVertex2f(-1,  1);
	glTexCoord2f((GLfloat)targetSource.left,  (GLfloat)targetSource.top  ); glVertex2f(-1, -1);
	glTexCoord2f((GLfloat)targetSource.right, (GLfloat)targetSource.top  ); glVertex2f( 1, -1);
	glTexCoord2f((GLfloat)targetSource.right, (GLfloat)targetSource.bottom); glVertex2f( 1,  1);
	glEnd();

	GL_REPORT_ERRORD();

	// Unbind texture from temporary framebuffer
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, 0, 0);

	// TODO: these good here?
    FramebufferManager::SetFramebuffer(0);
    VertexShaderManager::SetViewportChanged();
    DisableStage(0);

    GL_REPORT_ERRORD();

	// TODO: do this?
	//glBindTexture(GL_TEXTURE_2D, 0);
}

void TextureCache::TCacheEntry::Bind(unsigned int stage)
{
	glBindTexture(GL_TEXTURE_2D, texture);
}

TextureCache::TCacheEntryBase* TextureCache::CreateTexture(unsigned int width,
	unsigned int height, unsigned int expanded_width,
	unsigned int tex_levels, PC_TexFormat pcfmt)
{
	TCacheEntry &entry = *new TCacheEntry;

	int gl_format = 0;
	int gl_iformat = 0;
	int gl_type = 0;

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

	entry.gl_format = gl_format;
	entry.gl_iformat = gl_iformat;
	entry.gl_type = gl_type;

	// ok?
	//Load(width, height, expanded_width, level);

	return &entry;
}

TextureCache::TCacheEntryBase* TextureCache::CreateRenderTargetTexture(int scaled_tex_w, int scaled_tex_h)
{
	TCacheEntry &entry = *new TCacheEntry;

	entry.isDynamic = false;

	glBindTexture(GL_TEXTURE_2D, entry.texture);
	GL_REPORT_ERRORD();

	const GLenum gl_format = GL_RGBA,
		gl_iformat = 4,
		gl_type = GL_UNSIGNED_BYTE;
	glTexImage2D(GL_TEXTURE_2D, 0, gl_iformat, scaled_tex_w, scaled_tex_w, 0, gl_format, gl_type, NULL);
	
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

	return &entry;
}

void TextureCache::TCacheEntry::SetTextureParameters(TexMode0 &newmode, TexMode1 &newmode1)
{
    mode = newmode;
	//mode1 = newmode1;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
		            (newmode.mag_filter || g_Config.bForceFiltering) ? GL_LINEAR : GL_NEAREST);

    if (bHaveMipMaps) 
	{
		if (g_ActiveConfig.bForceFiltering && newmode.min_filter < 4)
            mode.min_filter += 4; // take equivalent forced linear

        int filt = newmode.min_filter;            
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, c_MinLinearFilter[filt & 7]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, newmode1.min_lod >> 4);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, newmode1.max_lod >> 4);
		glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, (newmode.lod_bias/32.0f));

    }
    else
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
		                (g_ActiveConfig.bForceFiltering || newmode.min_filter >= 4) ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, c_WrapSettings[newmode.wrap_s]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, c_WrapSettings[newmode.wrap_t]);
    
    if (g_Config.iMaxAnisotropy >= 1)
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, (float)(1 << g_ActiveConfig.iMaxAnisotropy));

}

TextureCache::~TextureCache()
{
    if (s_TempFramebuffer)
	{
        glDeleteFramebuffersEXT(1, (GLuint *)&s_TempFramebuffer);
        s_TempFramebuffer = 0;
    }
}

void TextureCache::DisableStage(int stage)
{
	glActiveTexture(GL_TEXTURE0 + stage);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_RECTANGLE_ARB);
}

}
