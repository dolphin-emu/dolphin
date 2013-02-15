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
#include "ProgramShaderCache.h"
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

static SHADER s_ColorMatrixProgram;
static SHADER s_DepthMatrixProgram;
static GLuint s_ColorMatrixUniform;
static GLuint s_DepthMatrixUniform;
static u32 s_ColorCbufid;
static u32 s_DepthCbufid;

static u32 s_Textures[8];
static u32 s_ActiveTexture;

struct VBOCache {
	GLuint vbo;
	GLuint vao;
	TargetRectangle targetSource;
};
static std::map<u64,VBOCache> s_VBO;

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
	glActiveTexture(GL_TEXTURE0+9);
	glBindTexture(textarget, tex);
	glGetTexImage(textarget, level, GL_BGRA, GL_UNSIGNED_BYTE, &data[0]);
	glBindTexture(textarget, 0);
	TextureCache::SetStage();
	
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
		for(int i=0; i<8; i++)
			if(s_Textures[i] == texture)
				s_Textures[i] = 0;
		glDeleteTextures(1, &texture);
		texture = 0;
	}
}

TextureCache::TCacheEntry::TCacheEntry()
{
	glGenTextures(1, &texture);
	currmode.hex = 0;
	currmode1.hex = 0;
	GL_REPORT_ERRORD();
}

void TextureCache::TCacheEntry::Bind(unsigned int stage)
{
	// TODO: is this already done somewhere else?
	TexMode0 &tm0 = bpmem.tex[stage >> 2].texMode0[stage & 3];
	TexMode1 &tm1 = bpmem.tex[stage >> 2].texMode1[stage & 3];
	
	if(currmode.hex != tm0.hex || currmode1.hex != tm1.hex)
	{
		if(s_ActiveTexture != stage)
			glActiveTexture(GL_TEXTURE0 + stage);
		if(s_Textures[stage] != texture)
			glBindTexture(GL_TEXTURE_2D, texture);
		
		SetTextureParameters(tm0, tm1);
		s_ActiveTexture = stage;
		s_Textures[stage] = texture;
	} 
	else if (s_Textures[stage] != texture) 
	{
		if(s_ActiveTexture != stage)
			glActiveTexture(GL_TEXTURE0 + stage);
		glBindTexture(GL_TEXTURE_2D, texture);
		s_ActiveTexture = stage;
		s_Textures[stage] = texture;
	}
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
			gl_iformat = GL_RGBA;
			gl_type = GL_UNSIGNED_BYTE;
			break;

		case PC_TEX_FMT_RGBA32:
			gl_format = GL_RGBA;
			gl_iformat = GL_RGBA;
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

	entry.m_tex_levels = tex_levels;

	return &entry;
}

void TextureCache::TCacheEntry::Load(unsigned int stage, unsigned int width, unsigned int height,
	unsigned int expanded_width, unsigned int level, bool autogen_mips)
{
	if(s_ActiveTexture != stage)
		glActiveTexture(GL_TEXTURE0 + stage);
	if(s_Textures[stage] != texture)
		glBindTexture(GL_TEXTURE_2D, texture);
	s_ActiveTexture = stage;
	s_Textures[stage] = texture;
	
	if(level == 0 && m_tex_levels != 0)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, m_tex_levels - 1);

	if (pcfmt != PC_TEX_FMT_DXT1)
	{
	    if (expanded_width != width)
			glPixelStorei(GL_UNPACK_ROW_LENGTH, expanded_width);

		glTexImage2D(GL_TEXTURE_2D, level, gl_iformat, width, height, 0, gl_format, gl_type, temp);
	    
		if (autogen_mips)
			glGenerateMipmap(GL_TEXTURE_2D);

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
	glActiveTexture(GL_TEXTURE0+9);
	glBindTexture(GL_TEXTURE_2D, entry->texture);
	GL_REPORT_ERRORD();

	const GLenum
		gl_format = GL_RGBA,
		gl_iformat = GL_RGBA,
		gl_type = GL_UNSIGNED_BYTE;
		
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	entry->m_tex_levels = 1;

	glTexImage2D(GL_TEXTURE_2D, 0, gl_iformat, scaled_tex_w, scaled_tex_h, 0, gl_format, gl_type, NULL);
	
	glBindTexture(GL_TEXTURE_2D, 0);
	
	SetStage();
	
	GL_REPORT_ERRORD();

	return entry;
}

void TextureCache::TCacheEntry::FromRenderTarget(u32 dstAddr, unsigned int dstFormat,
	unsigned int srcFormat, const EFBRectangle& srcRect,
	bool isIntensity, bool scaleByHalf, unsigned int cbufid,
	const float *colmat)
{
	g_renderer->ResetAPIState(); // reset any game specific settings
	
	// Make sure to resolve anything we need to read from.
	const GLuint read_texture = (srcFormat == PIXELFMT_Z24) ?
		FramebufferManager::ResolveAndGetDepthTarget(srcRect) :
		FramebufferManager::ResolveAndGetRenderTarget(srcRect);

    GL_REPORT_ERRORD();

	if (type != TCET_EC_DYNAMIC || g_ActiveConfig.bCopyEFBToTexture)
	{
		if (s_TempFramebuffer == 0)
			glGenFramebuffers(1, (GLuint*)&s_TempFramebuffer);

		FramebufferManager::SetFramebuffer(s_TempFramebuffer);
		// Bind texture to temporary framebuffer
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
		GL_REPORT_FBO_ERROR();
		GL_REPORT_ERRORD();
		
		glActiveTexture(GL_TEXTURE0+9);
		glBindTexture(GL_TEXTURE_RECTANGLE, read_texture);

		glViewport(0, 0, virtual_width, virtual_height);

		if(srcFormat == PIXELFMT_Z24) {
			s_DepthMatrixProgram.Bind();
			if(s_DepthCbufid != cbufid)
				glUniform4fv(s_DepthMatrixUniform, 5, colmat);
			s_DepthCbufid = cbufid;
		} else {
			s_ColorMatrixProgram.Bind();
			if(s_ColorCbufid != cbufid)
				glUniform4fv(s_ColorMatrixUniform, 7, colmat);
			s_ColorCbufid = cbufid;
		}
		GL_REPORT_ERRORD();

		TargetRectangle targetSource = g_renderer->ConvertEFBRectangle(srcRect);
		GL_REPORT_ERRORD();
		
		// should be unique enough, if not, vbo will "only" be uploaded to much
		u64 targetSourceHash = u64(targetSource.left)<<48 | u64(targetSource.top)<<32 | u64(targetSource.right)<<16 | u64(targetSource.bottom);
		std::map<u64, VBOCache>::iterator vbo_it = s_VBO.find(targetSourceHash);
		
		if(vbo_it == s_VBO.end()) {
			VBOCache item;
			item.targetSource.bottom = -1;
			item.targetSource.top = -1;
			item.targetSource.left = -1;
			item.targetSource.right = -1;
			glGenBuffers(1, &item.vbo);
			glGenVertexArrays(1, &item.vao);
			
			glBindBuffer(GL_ARRAY_BUFFER, item.vbo);
			glBindVertexArray(item.vao);
			
			glEnableVertexAttribArray(SHADER_POSITION_ATTRIB);
			glVertexAttribPointer(SHADER_POSITION_ATTRIB, 2, GL_FLOAT, 0, sizeof(GLfloat)*4, (GLfloat*)NULL);
			glEnableVertexAttribArray(SHADER_TEXTURE0_ATTRIB);
			glVertexAttribPointer(SHADER_TEXTURE0_ATTRIB, 2, GL_FLOAT, 0, sizeof(GLfloat)*4, (GLfloat*)NULL+2);
			
			vbo_it = s_VBO.insert(std::pair<u64,VBOCache>(targetSourceHash, item)).first;
		}
		if(!(vbo_it->second.targetSource == targetSource)) {
			GLfloat vertices[] = {
				-1.f, 1.f,
				(GLfloat)targetSource.left, (GLfloat)targetSource.bottom,
				-1.f, -1.f,
				(GLfloat)targetSource.left, (GLfloat)targetSource.top,
				1.f, -1.f,
				(GLfloat)targetSource.right, (GLfloat)targetSource.top,
				1.f, 1.f,
				(GLfloat)targetSource.right, (GLfloat)targetSource.bottom
			};
			
			glBindBuffer(GL_ARRAY_BUFFER, vbo_it->second.vbo);
			glBufferData(GL_ARRAY_BUFFER, 4*4*sizeof(GLfloat), vertices, GL_STREAM_DRAW);
			
			vbo_it->second.targetSource = targetSource;
		} 

		glBindVertexArray(vbo_it->second.vao);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);

		GL_REPORT_ERRORD();

		// Unbind texture from temporary framebuffer
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
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

    GL_REPORT_ERRORD();

    if (g_ActiveConfig.bDumpEFBTarget)
    {
		static int count = 0;
		SaveTexture(StringFromFormat("%sefb_frame_%i.tga", File::GetUserPath(D_DUMPTEXTURES_IDX).c_str(),
			count++).c_str(), GL_TEXTURE_2D, texture, virtual_width, virtual_height, 0);
    }

	g_renderer->RestoreAPIState();
}

void TextureCache::TCacheEntry::SetTextureParameters(const TexMode0 &newmode, const TexMode1 &newmode1)
{
	currmode = newmode;
	currmode1 = newmode1;
	
	// TODO: not used anywhere
	TexMode0 mode = newmode;
	//mode1 = newmode1;

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
		(newmode.mag_filter || g_Config.bForceFiltering) ? GL_LINEAR : GL_NEAREST);

	if (m_tex_levels != 1) 
	{
		// TODO: not used anywhere
		if (g_ActiveConfig.bForceFiltering && newmode.min_filter < 4)
			mode.min_filter += 4; // take equivalent forced linear

		int filt = newmode.min_filter;            
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, c_MinLinearFilter[filt & 7]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, newmode1.min_lod >> 4);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, newmode1.max_lod >> 4);
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

TextureCache::TextureCache()
{
	const char *pColorMatrixProg = 
		"uniform sampler2DRect samp9;\n"
		"uniform vec4 colmat[7];\n"
		"in vec2 uv0;\n"
		"out vec4 ocol0;\n"
		"\n"
		"void main(){\n"
		"	vec4 texcol = texture2DRect(samp9, uv0);\n"
		"	texcol = round(texcol * colmat[5]) * colmat[6];\n"
		"	ocol0 = texcol * mat4(colmat[0], colmat[1], colmat[2], colmat[3]) + colmat[4];\n"
		"}\n";

	const char *pDepthMatrixProg =
		"uniform sampler2DRect samp9;\n"
		"uniform vec4 colmat[5];\n"
		"in vec2 uv0;\n"
		"out vec4 ocol0;\n"
		"\n"
		"void main(){\n"
		"	vec4 texcol = texture2DRect(samp9, uv0);\n"
		"	vec4 EncodedDepth = fract((texcol.r * (16777215.0f/16777216.0f)) * vec4(1.0f,256.0f,256.0f*256.0f,1.0f));\n"
		"	texcol = round(EncodedDepth * (16777216.0f/16777215.0f) * vec4(255.0f,255.0f,255.0f,15.0f)) / vec4(255.0f,255.0f,255.0f,15.0f);\n"
		"	ocol0 = texcol * mat4(colmat[0], colmat[1], colmat[2], colmat[3]) + colmat[4];"
		"}\n";

	
	const char *VProgram =
		"in vec2 rawpos;\n"
		"in vec2 tex0;\n"
		"out vec2 uv0;\n"
		"void main()\n"
		"{\n"
		"	uv0 = tex0;\n"
		"	gl_Position = vec4(rawpos,0,1);\n"
		"}\n";
		
	ProgramShaderCache::CompileShader(s_ColorMatrixProgram, VProgram, pColorMatrixProg);
	ProgramShaderCache::CompileShader(s_DepthMatrixProgram, VProgram, pDepthMatrixProg);
	
	s_ColorMatrixUniform = glGetUniformLocation(s_ColorMatrixProgram.glprogid, "colmat");
	s_DepthMatrixUniform = glGetUniformLocation(s_DepthMatrixProgram.glprogid, "colmat");
	s_ColorCbufid = -1;
	s_DepthCbufid = -1;
	
	s_ActiveTexture = -1;
	for(int i=0; i<8; i++)
		s_Textures[i] = -1;
}


TextureCache::~TextureCache()
{
	s_ColorMatrixProgram.Destroy();
	s_DepthMatrixProgram.Destroy();
	
	for(std::map<u64, VBOCache>::iterator it = s_VBO.begin(); it != s_VBO.end(); it++) {
		glDeleteBuffers(1, &it->second.vbo);
		glDeleteVertexArrays(1, &it->second.vao);
	}
	s_VBO.clear();
	
	if (s_TempFramebuffer)
	{
		glDeleteFramebuffers(1, (GLuint*)&s_TempFramebuffer);
		s_TempFramebuffer = 0;
	}
}

void TextureCache::DisableStage(unsigned int stage)
{
}

void TextureCache::SetStage ()
{
	glActiveTexture(GL_TEXTURE0 + s_ActiveTexture);
}


}
