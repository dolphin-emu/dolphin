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

TextureCache::TCacheEntry::TCacheEntry(const TCacheEntryConfig& _config)
: TCacheEntryBase(_config)
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
	return SaveTexture(filename, GL_TEXTURE_2D_ARRAY, texture, config.width, config.height, level);
}

TextureCache::TCacheEntryBase* TextureCache::CreateTexture(const TCacheEntryConfig& config)
{
	TCacheEntry* entry = new TCacheEntry(config);

	glActiveTexture(GL_TEXTURE0+9);
	glBindTexture(GL_TEXTURE_2D_ARRAY, entry->texture);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, config.levels - 1);

	if (config.rendertarget)
	{
		for (u32 level = 0; level <= config.levels; level++)
		{
			glTexImage3D(GL_TEXTURE_2D_ARRAY, level, GL_RGBA, config.width, config.height, config.layers, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		}
		glGenFramebuffers(1, &entry->framebuffer);
		FramebufferManager::SetFramebuffer(entry->framebuffer);
		FramebufferManager::FramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_ARRAY, entry->texture, 0);
	}

	TextureCache::SetStage();
	return entry;
}

void TextureCache::TCacheEntry::Load(unsigned int width, unsigned int height,
	unsigned int expanded_width, unsigned int level)
{
	if (level >= config.levels)
		PanicAlert("Texture only has %d levels, can't update level %d", config.levels, level);
	if (width != std::max(1u, config.width >> level) || height != std::max(1u, config.height >> level))
		PanicAlert("size of level %d must be %dx%d, but %dx%d requested",
		           level, std::max(1u, config.width >> level), std::max(1u, config.height >> level), width, height);

	glActiveTexture(GL_TEXTURE0+9);
	glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

	if (expanded_width != width)
		glPixelStorei(GL_UNPACK_ROW_LENGTH, expanded_width);

	glTexImage3D(GL_TEXTURE_2D_ARRAY, level, GL_RGBA, width, height, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, temp);

	if (expanded_width != width)
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	TextureCache::SetStage();
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

	FramebufferManager::SetFramebuffer(framebuffer);

	OpenGL_BindAttributelessVAO();

	glActiveTexture(GL_TEXTURE0+9);
	glBindTexture(GL_TEXTURE_2D_ARRAY, read_texture);

	glViewport(0, 0, config.width, config.height);

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

		size_in_bytes = (u32)encoded_size;

		TextureCache::MakeRangeDynamic(addr,encoded_size);

		hash = new_hash;
	}

	FramebufferManager::SetFramebuffer(0);

	if (g_ActiveConfig.bDumpEFBTarget)
	{
		static int count = 0;
		SaveTexture(StringFromFormat("%sefb_frame_%i.png", File::GetUserPath(D_DUMPTEXTURES_IDX).c_str(),
			count++), GL_TEXTURE_2D_ARRAY, texture, config.width, config.height, 0);
	}

	g_renderer->RestoreAPIState();
}

TextureCache::TextureCache()
{
	CompileShaders();

	s_ActiveTexture = -1;
	for (auto& gtex : s_Textures)
		gtex = -1;
}


TextureCache::~TextureCache()
{
	DeleteShaders();
}

void TextureCache::DisableStage(unsigned int stage)
{
}

void TextureCache::SetStage()
{
	// -1 is the initial value as we don't know which texture should be bound
	if (s_ActiveTexture != (u32)-1)
		glActiveTexture(GL_TEXTURE0 + s_ActiveTexture);
}

void TextureCache::CompileShaders()
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
		"	vec4 texcol = texture(samp9, vec3(f_uv0.xy, %s));\n"

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

	const char *VProgram =
		"out vec3 %s_uv0;\n"
		"SAMPLER_BINDING(9) uniform sampler2DArray samp9;\n"
		"uniform vec4 copy_position;\n" // left, top, right, bottom
		"void main()\n"
		"{\n"
		"	vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);\n"
		"	%s_uv0 = vec3(mix(copy_position.xy, copy_position.zw, rawpos) / vec2(textureSize(samp9, 0).xy), 0.0);\n"
		"	gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);\n"
		"}\n";

	const char *GProgram = g_ActiveConfig.iStereoMode > 0 ?
		"layout(triangles) in;\n"
		"layout(triangle_strip, max_vertices = 6) out;\n"
		"in vec3 v_uv0[3];\n"
		"out vec3 f_uv0;\n"
		"SAMPLER_BINDING(9) uniform sampler2DArray samp9;\n"
		"void main()\n"
		"{\n"
		"	int layers = textureSize(samp9, 0).z;\n"
		"	for (int layer = 0; layer < layers; ++layer) {\n"
		"		for (int i = 0; i < 3; ++i) {\n"
		"			f_uv0 = vec3(v_uv0[i].xy, layer);\n"
		"			gl_Position = gl_in[i].gl_Position;\n"
		"			gl_Layer = layer;\n"
		"			EmitVertex();\n"
		"		}\n"
		"		EndPrimitive();\n"
		"	}\n"
		"}\n" : nullptr;

	const char* prefix = (GProgram == nullptr) ? "f" : "v";
	const char* depth_layer = (g_ActiveConfig.bStereoEFBMonoDepth) ? "0.0" : "f_uv0.z";

	ProgramShaderCache::CompileShader(s_ColorMatrixProgram, StringFromFormat(VProgram, prefix, prefix).c_str(), pColorMatrixProg, GProgram);
	ProgramShaderCache::CompileShader(s_DepthMatrixProgram, StringFromFormat(VProgram, prefix, prefix).c_str(), StringFromFormat(pDepthMatrixProg, depth_layer).c_str(), GProgram);

	s_ColorMatrixUniform = glGetUniformLocation(s_ColorMatrixProgram.glprogid, "colmat");
	s_DepthMatrixUniform = glGetUniformLocation(s_DepthMatrixProgram.glprogid, "colmat");
	s_ColorCbufid = -1;
	s_DepthCbufid = -1;

	s_ColorCopyPositionUniform = glGetUniformLocation(s_ColorMatrixProgram.glprogid, "copy_position");
	s_DepthCopyPositionUniform = glGetUniformLocation(s_DepthMatrixProgram.glprogid, "copy_position");
}

void TextureCache::DeleteShaders()
{
	s_ColorMatrixProgram.Destroy();
	s_DepthMatrixProgram.Destroy();
}

}
