// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>
#include <fstream>
#include <vector>

#ifdef _WIN32
#include <intrin.h>
#endif

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/MemoryUtil.h"
#include "Common/StringUtil.h"

#include "Core/HW/Memmap.h"

#include "VideoBackends/OGL/FramebufferManager.h"
#include "VideoBackends/OGL/GLInterfaceBase.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/TextureCache.h"
#include "VideoBackends/OGL/TextureConverter.h"

#include "VideoCommon/BPStructs.h"
#include "VideoCommon/HiresTextures.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoConfig.h"

namespace OGL
{

static SHADER s_ColorMatrixProgram;
static SHADER s_DepthMatrixProgram;
static GLuint s_ColorMatrixUniform;
static GLuint s_DepthMatrixUniform;
static GLuint s_ColorCopyPositionUniform;
static GLuint s_DepthCopyPositionUniform;
static u32 s_ColorCbufid;
static u32 s_DepthCbufid;

static u32 s_Textures[8];
static u32 s_ActiveTexture;

bool SaveTexture(const std::string& filename, u32 textarget, u32 tex, int virtual_width, int virtual_height, unsigned int level)
{
	if (GLInterface->GetMode() != GLInterfaceMode::MODE_OPENGL)
		return false;
	int width = std::max(virtual_width >> level, 1);
	int height = std::max(virtual_height >> level, 1);
	std::vector<u8> data(width * height * 4);
	glActiveTexture(GL_TEXTURE0+9);
	glBindTexture(textarget, tex);
	glGetTexImage(textarget, level, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
	glBindTexture(textarget, 0);
	TextureCache::SetStage();

	return TextureToPng(data.data(), width * 4, filename, width, height, true);
}

TextureCache::TCacheEntry::~TCacheEntry()
{
	if (texture)
	{
		for (auto& gtex : s_Textures)
			if (gtex == texture)
				gtex = 0;
		glDeleteTextures(1, &texture);
		texture = 0;
	}

	if (framebuffer)
	{
		glDeleteFramebuffers(1, &framebuffer);
		framebuffer = 0;
	}
}

TextureCache::TCacheEntry::TCacheEntry()
{
	glGenTextures(1, &texture);

	framebuffer = 0;
}

void TextureCache::TCacheEntry::Bind(unsigned int stage)
{
	if (s_Textures[stage] != texture)
	{
		if (s_ActiveTexture != stage)
		{
			glActiveTexture(GL_TEXTURE0 + stage);
			s_ActiveTexture = stage;
		}

		glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
		s_Textures[stage] = texture;
	}
}

bool TextureCache::TCacheEntry::Save(const std::string& filename, unsigned int level)
{
	return SaveTexture(filename, GL_TEXTURE_2D_ARRAY, texture, virtual_width, virtual_height, level);
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

	glActiveTexture(GL_TEXTURE0+9);
	glBindTexture(GL_TEXTURE_2D_ARRAY, entry.texture);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, tex_levels - 1);

	entry.Load(width, height, expanded_width, 0);

	// This isn't needed as Load() also reset the stage in the end
	//TextureCache::SetStage();

	return &entry;
}

void TextureCache::TCacheEntry::Load(unsigned int width, unsigned int height,
	unsigned int expanded_width, unsigned int level)
{
	if (pcfmt != PC_TEX_FMT_DXT1)
	{
		glActiveTexture(GL_TEXTURE0+9);
		glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

		if (expanded_width != width)
			glPixelStorei(GL_UNPACK_ROW_LENGTH, expanded_width);

		glTexImage3D(GL_TEXTURE_2D_ARRAY, level, gl_iformat, width, height, 1, 0, gl_format, gl_type, temp);

		if (expanded_width != width)
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	}
	else
	{
		PanicAlert("PC_TEX_FMT_DXT1 support disabled");
		//glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
			//width, height, 0, expanded_width * expanded_height/2, temp);
	}
	TextureCache::SetStage();
}

TextureCache::TCacheEntryBase* TextureCache::CreateRenderTargetTexture(
	unsigned int scaled_tex_w, unsigned int scaled_tex_h)
{
	TCacheEntry *const entry = new TCacheEntry;
	glActiveTexture(GL_TEXTURE0+9);
	glBindTexture(GL_TEXTURE_2D_ARRAY, entry->texture);

	const GLenum
		gl_format = GL_RGBA,
		gl_iformat = GL_RGBA,
		gl_type = GL_UNSIGNED_BYTE;

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);

	entry->num_layers = FramebufferManager::GetEFBLayers();

	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, gl_iformat, scaled_tex_w, scaled_tex_h, entry->num_layers, 0, gl_format, gl_type, nullptr);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

	glGenFramebuffers(1, &entry->framebuffer);
	FramebufferManager::SetFramebuffer(entry->framebuffer);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, entry->texture, 0);

	SetStage();

	return entry;
}

void TextureCache::TCacheEntry::FromRenderTarget(u32 dstAddr, unsigned int dstFormat,
	PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
	bool isIntensity, bool scaleByHalf, unsigned int cbufid,
	const float *colmat)
{
	g_renderer->ResetAPIState(); // reset any game specific settings

	// Make sure to resolve anything we need to read from.
	const GLuint read_texture = (srcFormat == PEControl::Z24) ?
		FramebufferManager::ResolveAndGetDepthTarget(srcRect) :
		FramebufferManager::ResolveAndGetRenderTarget(srcRect);

	if (type != TCET_EC_DYNAMIC || g_ActiveConfig.bCopyEFBToTexture)
	{
		FramebufferManager::SetFramebuffer(framebuffer);

		glActiveTexture(GL_TEXTURE0+9);
		glBindTexture(GL_TEXTURE_2D_ARRAY, read_texture);

		glViewport(0, 0, virtual_width, virtual_height);

		GLuint uniform_location;
		if (srcFormat == PEControl::Z24)
		{
			s_DepthMatrixProgram.Bind();
			if (s_DepthCbufid != cbufid)
				glUniform4fv(s_DepthMatrixUniform, 5, colmat);
			s_DepthCbufid = cbufid;
			uniform_location = s_DepthCopyPositionUniform;
		}
		else
		{
			s_ColorMatrixProgram.Bind();
			if (s_ColorCbufid != cbufid)
				glUniform4fv(s_ColorMatrixUniform, 7, colmat);
			s_ColorCbufid = cbufid;
			uniform_location = s_ColorCopyPositionUniform;
		}

		TargetRectangle R = g_renderer->ConvertEFBRectangle(srcRect);
		glUniform4f(uniform_location, static_cast<float>(R.left), static_cast<float>(R.top),
			static_cast<float>(R.right), static_cast<float>(R.bottom));

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}

	if (false == g_ActiveConfig.bCopyEFBToTexture)
	{
		int encoded_size = TextureConverter::EncodeToRamFromTexture(
			addr,
			read_texture,
			srcFormat == PEControl::Z24,
			isIntensity,
			dstFormat,
			scaleByHalf,
			srcRect);

		u8* dst = Memory::GetPointer(addr);
		u64 const new_hash = GetHash64(dst,encoded_size,g_ActiveConfig.iSafeTextureCache_ColorSamples);

		// Mark texture entries in destination address range dynamic unless caching is enabled and the texture entry is up to date
		if (!g_ActiveConfig.bEFBCopyCacheEnable)
			TextureCache::MakeRangeDynamic(addr,encoded_size);
		else if (!TextureCache::Find(addr, new_hash))
			TextureCache::MakeRangeDynamic(addr,encoded_size);

		hash = new_hash;
	}

	FramebufferManager::SetFramebuffer(0);

	if (g_ActiveConfig.bDumpEFBTarget)
	{
		static int count = 0;
		SaveTexture(StringFromFormat("%sefb_frame_%i.png", File::GetUserPath(D_DUMPTEXTURES_IDX).c_str(),
			count++), GL_TEXTURE_2D_ARRAY, texture, virtual_width, virtual_height, 0);
	}

	g_renderer->RestoreAPIState();
}

TextureCache::TextureCache()
{
	const char *pColorMatrixProg =
		"SAMPLER_BINDING(9) uniform sampler2DArray samp9;\n"
		"uniform vec4 colmat[7];\n"
		"in vec3 f_uv0;\n"
		"out vec4 ocol0;\n"
		"\n"
		"void main(){\n"
		"	vec4 texcol = texture(samp9, f_uv0);\n"
		"	texcol = round(texcol * colmat[5]) * colmat[6];\n"
		"	ocol0 = texcol * mat4(colmat[0], colmat[1], colmat[2], colmat[3]) + colmat[4];\n"
		"}\n";

	const char *pDepthMatrixProg =
		"SAMPLER_BINDING(9) uniform sampler2DArray samp9;\n"
		"uniform vec4 colmat[5];\n"
		"in vec3 f_uv0;\n"
		"out vec4 ocol0;\n"
		"\n"
		"void main(){\n"
		"	vec4 texcol = texture(samp9, f_uv0);\n"

		// 255.99998474121 = 16777215/16777216*256
		"	float workspace = texcol.x * 255.99998474121;\n"

		"	texcol.x = floor(workspace);\n"         // x component

		"	workspace = workspace - texcol.x;\n"    // subtract x component out
		"	workspace = workspace * 256.0;\n"       // shift left 8 bits
		"	texcol.y = floor(workspace);\n"         // y component

		"	workspace = workspace - texcol.y;\n"    // subtract y component out
		"	workspace = workspace * 256.0;\n"       // shift left 8 bits
		"	texcol.z = floor(workspace);\n"         // z component

		"	texcol.w = texcol.x;\n"                 // duplicate x into w

		"	texcol = texcol / 255.0;\n"             // normalize components to [0.0..1.0]

		"	texcol.w = texcol.w * 15.0;\n"
		"	texcol.w = floor(texcol.w);\n"
		"	texcol.w = texcol.w / 15.0;\n"          // w component

		"	ocol0 = texcol * mat4(colmat[0], colmat[1], colmat[2], colmat[3]) + colmat[4];\n"
		"}\n";

	const char *VProgram = (g_ActiveConfig.bStereo) ?
		"out vec2 v_uv0;\n"
		"SAMPLER_BINDING(9) uniform sampler2DArray samp9;\n"
		"uniform vec4 copy_position;\n" // left, top, right, bottom
		"void main()\n"
		"{\n"
		"	vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);\n"
		"	v_uv0 = mix(copy_position.xy, copy_position.zw, rawpos) / vec2(textureSize(samp9, 0).xy);\n"
		"	gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);\n"
		"}\n" :
		"out vec3 f_uv0;\n"
		"SAMPLER_BINDING(9) uniform sampler2DArray samp9;\n"
		"uniform vec4 copy_position;\n" // left, top, right, bottom
		"void main()\n"
		"{\n"
		"	vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);\n"
		"	f_uv0.xy = mix(copy_position.xy, copy_position.zw, rawpos) / vec2(textureSize(samp9, 0).xy);\n"
		"	f_uv0.z = 0;\n"
		"	gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);\n"
		"}\n";

	const char *GProgram = (g_ActiveConfig.bStereo) ?
		"layout(triangles) in;\n"
		"layout(triangle_strip, max_vertices = 6) out;\n"
		"in vec2 v_uv0[];\n"
		"out vec3 f_uv0;\n"
		"SAMPLER_BINDING(9) uniform sampler2DArray samp9;\n"
		"void main()\n"
		"{\n"
		"	int layers = textureSize(samp9, 0).z;\n"
		"	for (int layer = 0; layer < layers; ++layer) {\n"
		"		for (int i = 0; i < gl_in.length(); ++i) {\n"
		"			f_uv0 = vec3(v_uv0[i], layer);\n"
		"			gl_Position = gl_in[i].gl_Position;\n"
		"			gl_Layer = layer;\n"
		"			EmitVertex();\n"
		"		}\n"
		"		EndPrimitive();\n"
		"	}\n"
		"}\n" : nullptr;

	ProgramShaderCache::CompileShader(s_ColorMatrixProgram, VProgram, pColorMatrixProg, GProgram);
	ProgramShaderCache::CompileShader(s_DepthMatrixProgram, VProgram, pDepthMatrixProg, GProgram);

	s_ColorMatrixUniform = glGetUniformLocation(s_ColorMatrixProgram.glprogid, "colmat");
	s_DepthMatrixUniform = glGetUniformLocation(s_DepthMatrixProgram.glprogid, "colmat");
	s_ColorCbufid = -1;
	s_DepthCbufid = -1;

	s_ColorCopyPositionUniform = glGetUniformLocation(s_ColorMatrixProgram.glprogid, "copy_position");
	s_DepthCopyPositionUniform = glGetUniformLocation(s_DepthMatrixProgram.glprogid, "copy_position");

	s_ActiveTexture = -1;
	for (auto& gtex : s_Textures)
		gtex = -1;
}


TextureCache::~TextureCache()
{
	s_ColorMatrixProgram.Destroy();
	s_DepthMatrixProgram.Destroy();
}

void TextureCache::DisableStage(unsigned int stage)
{
}

void TextureCache::SetStage ()
{
	// -1 is the initial value as we don't know which texture should be bound
	if (s_ActiveTexture != (u32)-1)
		glActiveTexture(GL_TEXTURE0 + s_ActiveTexture);
}

}
