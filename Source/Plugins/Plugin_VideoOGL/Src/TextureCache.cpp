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

static FRAGMENTSHADER s_ColorMatrixProgram;
static FRAGMENTSHADER s_DepthMatrixProgram;
static GLuint s_ColorMatrixUniform;
static GLuint s_DepthMatrixUniform;
static u32 s_ColorCbufid;
static u32 s_DepthCbufid;
static VERTEXSHADER s_vProgram;

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
	currmode.hex = 0;
	currmode1.hex = 0;
	GL_REPORT_ERRORD();
}

void TextureCache::TCacheEntry::Bind(unsigned int stage)
{
	glBindTexture(GL_TEXTURE_2D, texture);
	GL_REPORT_ERRORD();

	// TODO: is this already done somewhere else?
	TexMode0 &tm0 = bpmem.tex[stage >> 2].texMode0[stage & 3];
	TexMode1 &tm1 = bpmem.tex[stage >> 2].texMode1[stage & 3];
	
	if(currmode.hex != tm0.hex || currmode1.hex != tm1.hex)
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
			glGenFramebuffers(1, (GLuint*)&s_TempFramebuffer);

		FramebufferManager::SetFramebuffer(s_TempFramebuffer);
		// Bind texture to temporary framebuffer
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
		GL_REPORT_FBO_ERROR();
		GL_REPORT_ERRORD();

		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glEnable(GL_TEXTURE_RECTANGLE);
		glBindTexture(GL_TEXTURE_RECTANGLE, read_texture);

		glViewport(0, 0, virtual_width, virtual_height);

		if(srcFormat == PIXELFMT_Z24) {
			ProgramShaderCache::SetBothShaders(s_DepthMatrixProgram.glprogid, s_vProgram.glprogid);
			if(s_DepthCbufid != cbufid)
				glUniform4fv(s_DepthMatrixUniform, 5, colmat);
			s_DepthCbufid = cbufid;
		} else {
			ProgramShaderCache::SetBothShaders(s_ColorMatrixProgram.glprogid, s_vProgram.glprogid);
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
		
		// TODO: this after merging with graphic_update
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

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
	currmode = newmode;
	currmode1 = newmode1;
	
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

TextureCache::TextureCache()
{
	const char *pColorMatrixProg = 
		"#version 130\n"
		"#extension GL_ARB_texture_rectangle : enable\n"
		"uniform sampler2DRect samp0;\n"
		"uniform vec4 colmat[7];\n"
		"in vec2 uv0;\n"
		"out vec4 ocol0;\n"
		"\n"
		"void main(){\n"
		"	vec4 Temp0, Temp1;\n"
		"	vec4 K0 = vec4(0.5, 0.5, 0.5, 0.5);\n"
		"	Temp0 = texture2DRect(samp0, uv0);\n"
		"	Temp0 = Temp0 * colmat[5];\n"
		"	Temp0 = Temp0 + K0;\n"
		"	Temp0 = floor(Temp0);\n"
		"	Temp0 = Temp0 * colmat[6];\n"
		"	Temp1.x = dot(Temp0, colmat[0]);\n"
		"	Temp1.y = dot(Temp0, colmat[1]);\n"
		"	Temp1.z = dot(Temp0, colmat[2]);\n"
		"	Temp1.w = dot(Temp0, colmat[3]);\n"
		"	ocol0 = Temp1 + colmat[4];\n"
		"}\n";
	if (!PixelShaderCache::CompilePixelShader(s_ColorMatrixProgram, pColorMatrixProg))
	{
		ERROR_LOG(VIDEO, "Failed to create color matrix fragment program");
		s_ColorMatrixProgram.Destroy();
	}

	const char *pDepthMatrixProg =
		"#version 130\n"
		"#extension GL_ARB_texture_rectangle : enable\n"
		"uniform sampler2DRect samp0;\n"
		"uniform vec4 colmat[5];\n"
		"in vec2 uv0;\n"
		"out vec4 ocol0;\n"
		"\n"
		"void main(){\n"
		"	vec4 R0, R1, R2;\n"
		"	vec4 K0 = vec4(255.99998474121, 0.003921568627451, 256.0, 0.0);\n"
		"	vec4 K1 = vec4(15.0, 0.066666666666, 0.0, 0.0);\n"
		"	R2 = texture2DRect(samp0, uv0);\n"
		"	R0.x = R2.x * K0.x;\n"
		"	R0.x = floor(R0).x;\n"
		"	R0.yzw = (R0 - R0.x).yzw;\n"
		"	R0.yzw = (R0 * K0.z).yzw;\n"
		"	R0.y = floor(R0).y;\n"
		"	R0.zw = (R0 - R0.y).zw;\n"
		"	R0.zw = (R0 * K0.z).zw;\n"
		"	R0.z = floor(R0).z;\n"
		"	R0.w = R0.x;\n"
		"	R0 = R0 * K0.y;\n"
		"	R0.w = (R0 * K1.x).w;\n"
		"	R0.w = floor(R0).w;\n"
		"	R0.w = (R0 * K1.y).w;\n"
		"	R1.x = dot(R0, colmat[0]);\n"
		"	R1.y = dot(R0, colmat[1]);\n"
		"	R1.z = dot(R0, colmat[2]);\n"
		"	R1.w = dot(R0, colmat[3]);\n"
		"	ocol0 = R1 * colmat[4];\n"
		"}\n";

	if (!PixelShaderCache::CompilePixelShader(s_DepthMatrixProgram, pDepthMatrixProg))
	{
		ERROR_LOG(VIDEO, "Failed to create depth matrix fragment program");
		s_DepthMatrixProgram.Destroy();
	}
	
	const char *VProgram =
		"#version 130\n"
		"in vec2 rawpos;\n"
		"in vec2 texture0;\n"
		"out vec2 uv0;\n"
		"void main()\n"
		"{\n"
		"	uv0 = texture0;\n"
		"	gl_Position = vec4(rawpos,0,1);\n"
		"}\n";
	if (!VertexShaderCache::CompileVertexShader(s_vProgram, VProgram))
		ERROR_LOG(VIDEO, "Failed to create texture converter vertex program.");
	
	ProgramShaderCache::SetBothShaders(s_ColorMatrixProgram.glprogid, s_vProgram.glprogid);
	s_ColorMatrixUniform = glGetUniformLocation(ProgramShaderCache::GetCurrentProgram(), "colmat");
	ProgramShaderCache::SetBothShaders(s_DepthMatrixProgram.glprogid, s_vProgram.glprogid);
	s_DepthMatrixUniform = glGetUniformLocation(ProgramShaderCache::GetCurrentProgram(), "colmat");
	s_ColorCbufid = -1;
	s_DepthCbufid = -1;
}


TextureCache::~TextureCache()
{
	s_ColorMatrixProgram.Destroy();
	s_DepthMatrixProgram.Destroy();
	s_vProgram.Destroy();
	
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
	glActiveTexture(GL_TEXTURE0 + stage);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_RECTANGLE);
}

}
