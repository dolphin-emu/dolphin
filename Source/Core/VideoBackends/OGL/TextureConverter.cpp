// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// Fast image conversion using OpenGL shaders.

#include <string>

#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/HW/Memmap.h"

#include "VideoBackends/OGL/FramebufferManager.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/TextureCache.h"
#include "VideoBackends/OGL/TextureConverter.h"

#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/TextureConversionShader.h"
#include "VideoCommon/VideoConfig.h"


namespace OGL
{

namespace TextureConverter
{

using OGL::TextureCache;

static GLuint s_texConvFrameBuffer[2] = {0,0};
static GLuint s_srcTexture = 0; // for decoding from RAM
static GLuint s_dstTexture = 0; // for encoding to RAM

const int renderBufferWidth = 1024;
const int renderBufferHeight = 1024;

static SHADER s_rgbToYuyvProgram;
static int s_rgbToYuyvUniform_loc;

static SHADER s_yuyvToRgbProgram;

// Not all slots are taken - but who cares.
const u32 NUM_ENCODING_PROGRAMS = 64;
static SHADER s_encodingPrograms[NUM_ENCODING_PROGRAMS];
static int s_encodingUniforms[NUM_ENCODING_PROGRAMS];

static GLuint s_PBO = 0; // for readback with different strides

static void CreatePrograms()
{
	/* TODO: Accuracy Improvements
	 *
	 * This shader doesn't really match what the gamecube does interally in the
	 * copy pipeline.
	 *  1. It uses Opengl's built in filtering when yscaling, someone could work
	 *     out how the copypipeline does it's filtering and implement it correctly
	 *     in this shader.
	 *  2. Deflickering isn't implemented, a futher filtering over 3 lines.
	 *     Isn't really needed on non-interlaced monitors (and would lower quality;
	 *     But hey, accuracy!)
	 *  3. Flipper's YUYV conversion implements a 3 pixel horozontal blur on the
	 *     UV channels, centering the U channel on the Left pixel and the V channel
	 *     on the Right pixel.
	 *     The current implementation Centers both UV channels at the same place
	 *     inbetween the two Pixels, and only blurs over these two pixels.
	 */
	// Output is BGRA because that is slightly faster than RGBA.
	const char *VProgramRgbToYuyv =
		"out vec2 uv0;\n"
		"uniform vec4 copy_position;\n" // left, top, right, bottom
		"SAMPLER_BINDING(9) uniform sampler2D samp9;\n"
		"void main()\n"
		"{\n"
		"	vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);\n"
		"	gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);\n"
		"	uv0 = mix(copy_position.xy, copy_position.zw, rawpos) / vec2(textureSize(samp9, 0));\n"
		"}\n";
	const char *FProgramRgbToYuyv =
		"SAMPLER_BINDING(9) uniform sampler2D samp9;\n"
		"in vec2 uv0;\n"
		"out vec4 ocol0;\n"
		"void main()\n"
		"{\n"
		"	vec3 c0 = texture(samp9, (uv0 - dFdx(uv0) * 0.25)).rgb;\n"
		"	vec3 c1 = texture(samp9, (uv0 + dFdx(uv0) * 0.25)).rgb;\n"
		"	vec3 c01 = (c0 + c1) * 0.5;\n"
		"	vec3 y_const = vec3(0.257,0.504,0.098);\n"
		"	vec3 u_const = vec3(-0.148,-0.291,0.439);\n"
		"	vec3 v_const = vec3(0.439,-0.368,-0.071);\n"
		"	vec4 const3 = vec4(0.0625,0.5,0.0625,0.5);\n"
		"	ocol0 = vec4(dot(c1,y_const),dot(c01,u_const),dot(c0,y_const),dot(c01, v_const)) + const3;\n"
		"}\n";
	ProgramShaderCache::CompileShader(s_rgbToYuyvProgram, VProgramRgbToYuyv, FProgramRgbToYuyv);
	s_rgbToYuyvUniform_loc = glGetUniformLocation(s_rgbToYuyvProgram.glprogid, "copy_position");

	/* TODO: Accuracy Improvements
	 *
	 * The YVYU to RGB conversion here matches the RGB to YUYV done above, but
	 * if a game modifies or adds images to the XFB then it should be using the
	 * same algorithm as the flipper, and could result in slight colour inaccuracies
	 * when run back through this shader.
	 */
	const char *VProgramYuyvToRgb =
		"void main()\n"
		"{\n"
		"	vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);\n"
		"	gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);\n"
		"}\n";
	const char *FProgramYuyvToRgb =
		"SAMPLER_BINDING(9) uniform sampler2D samp9;\n"
		"in vec2 uv0;\n"
		"out vec4 ocol0;\n"
		"void main()\n"
		"{\n"
		"	ivec2 uv = ivec2(gl_FragCoord.xy);\n"
			// We switch top/bottom here. TODO: move this to screen blit.
		"	ivec2 ts = textureSize(samp9, 0);\n"
		"	vec4 c0 = texelFetch(samp9, ivec2(uv.x>>1, ts.y-uv.y-1), 0);\n"
		"	float y = mix(c0.b, c0.r, (uv.x & 1) == 1);\n"
		"	float yComp = 1.164 * (y - 0.0625);\n"
		"	float uComp = c0.g - 0.5;\n"
		"	float vComp = c0.a - 0.5;\n"
		"	ocol0 = vec4(yComp + (1.596 * vComp),\n"
		"		yComp - (0.813 * vComp) - (0.391 * uComp),\n"
		"		yComp + (2.018 * uComp),\n"
		"		1.0);\n"
		"}\n";
	ProgramShaderCache::CompileShader(s_yuyvToRgbProgram, VProgramYuyvToRgb, FProgramYuyvToRgb);
}

static SHADER &GetOrCreateEncodingShader(u32 format)
{
	if (format >= NUM_ENCODING_PROGRAMS)
	{
		PanicAlert("Unknown texture copy format: 0x%x\n", format);
		return s_encodingPrograms[0];
	}

	if (s_encodingPrograms[format].glprogid == 0)
	{
		const char* shader = TextureConversionShader::GenerateEncodingShader(format, API_OPENGL);

#if defined(_DEBUG) || defined(DEBUGFAST)
		if (g_ActiveConfig.iLog & CONF_SAVESHADERS && shader)
		{
			static int counter = 0;
			std::string filename = StringFromFormat("%senc_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), counter++);

			SaveData(filename, shader);
		}
#endif

		const char *VProgram =
			"void main()\n"
			"{\n"
			"	vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);\n"
			"	gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);\n"
			"}\n";

		ProgramShaderCache::CompileShader(s_encodingPrograms[format], VProgram, shader);

		s_encodingUniforms[format] = glGetUniformLocation(s_encodingPrograms[format].glprogid, "position");
	}
	return s_encodingPrograms[format];
}

void Init()
{
	glGenFramebuffers(2, s_texConvFrameBuffer);

	glActiveTexture(GL_TEXTURE0 + 9);
	glGenTextures(1, &s_srcTexture);
	glBindTexture(GL_TEXTURE_2D, s_srcTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

	glGenTextures(1, &s_dstTexture);
	glBindTexture(GL_TEXTURE_2D, s_dstTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, renderBufferWidth, renderBufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);


	FramebufferManager::SetFramebuffer(s_texConvFrameBuffer[0]);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_dstTexture, 0);
	FramebufferManager::SetFramebuffer(0);

	glGenBuffers(1, &s_PBO);

	CreatePrograms();
}

void Shutdown()
{
	glDeleteTextures(1, &s_srcTexture);
	glDeleteTextures(1, &s_dstTexture);
	glDeleteBuffers(1, &s_PBO);
	glDeleteFramebuffers(2, s_texConvFrameBuffer);

	s_rgbToYuyvProgram.Destroy();
	s_yuyvToRgbProgram.Destroy();

	for (auto& program : s_encodingPrograms)
		program.Destroy();

	s_srcTexture = 0;
	s_dstTexture = 0;
	s_PBO = 0;
	s_texConvFrameBuffer[0] = 0;
	s_texConvFrameBuffer[1] = 0;
}

static void EncodeToRamUsingShader(GLuint srcTexture,
						u8* destAddr, int dstWidth, int dstHeight, int readStride,
						bool linearFilter)
{


	// switch to texture converter frame buffer
	// attach render buffer as color destination
	FramebufferManager::SetFramebuffer(s_texConvFrameBuffer[0]);

	// set source texture
	glActiveTexture(GL_TEXTURE0+9);
	glBindTexture(GL_TEXTURE_2D, srcTexture);

	if (linearFilter)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}

	glViewport(0, 0, (GLsizei)dstWidth, (GLsizei)dstHeight);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	// .. and then read back the results.
	// TODO: make this less slow.

	int writeStride = bpmem.copyMipMapStrideChannels * 32;
	int dstSize = dstWidth*dstHeight*4;
	int readHeight = readStride / dstWidth / 4; // 4 bytes per pixel
	int readLoops = dstHeight / readHeight;

	if (writeStride != readStride && readLoops > 1)
	{
		// writing to a texture of a different size
		// also copy more then one block line, so the different strides matters
		// copy into one pbo first, map this buffer, and then memcpy into gc memory
		// in this way, we only have one vram->ram transfer, but maybe a bigger
		// cpu overhead because of the pbo
		glBindBuffer(GL_PIXEL_PACK_BUFFER, s_PBO);
		glBufferData(GL_PIXEL_PACK_BUFFER, dstSize, nullptr, GL_STREAM_READ);
		glReadPixels(0, 0, (GLsizei)dstWidth, (GLsizei)dstHeight, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
		u8* pbo = (u8*)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, dstSize, GL_MAP_READ_BIT);

		for (int i = 0; i < readLoops; i++)
		{
			memcpy(destAddr, pbo, readStride);
			pbo += readStride;
			destAddr += writeStride;
		}

		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	}
	else
	{
		glReadPixels(0, 0, (GLsizei)dstWidth, (GLsizei)dstHeight, GL_BGRA, GL_UNSIGNED_BYTE, destAddr);
	}
}

int EncodeToRamFromTexture(u32 address,GLuint source_texture, bool bFromZBuffer, bool bIsIntensityFmt, u32 copyfmt, int bScaleByHalf, const EFBRectangle& source)
{
	u32 format = copyfmt;

	if (bFromZBuffer)
	{
		format |= _GX_TF_ZTF;
		if (copyfmt == 11)
			format = GX_TF_Z16;
		else if (format < GX_TF_Z8 || format > GX_TF_Z24X8)
			format |= _GX_TF_CTF;
	}
	else
	{
		if (copyfmt > GX_TF_RGBA8 || (copyfmt < GX_TF_RGB565 && !bIsIntensityFmt))
			format |= _GX_TF_CTF;
	}

	SHADER& texconv_shader = GetOrCreateEncodingShader(format);

	u8 *dest_ptr = Memory::GetPointer(address);

	int width = (source.right - source.left) >> bScaleByHalf;
	int height = (source.bottom - source.top) >> bScaleByHalf;

	int size_in_bytes = TexDecoder_GetTextureSizeInBytes(width, height, format);

	u16 blkW = TexDecoder_GetBlockWidthInTexels(format) - 1;
	u16 blkH = TexDecoder_GetBlockHeightInTexels(format) - 1;
	u16 samples = TextureConversionShader::GetEncodedSampleCount(format);

	// only copy on cache line boundaries
	// extra pixels are copied but not displayed in the resulting texture
	s32 expandedWidth = (width + blkW) & (~blkW);
	s32 expandedHeight = (height + blkH) & (~blkH);

	texconv_shader.Bind();
	glUniform4i(s_encodingUniforms[format],
		source.left, source.top,
		expandedWidth, bScaleByHalf ? 2 : 1);

	int cacheBytes = 32;
	if ((format & 0x0f) == 6)
		cacheBytes = 64;

	int readStride = (expandedWidth * cacheBytes) /
		TexDecoder_GetBlockWidthInTexels(format);
	EncodeToRamUsingShader(source_texture,
		dest_ptr, expandedWidth / samples, expandedHeight, readStride,
		bScaleByHalf > 0 && !bFromZBuffer);
	return size_in_bytes; // TODO: D3D11 is calculating this value differently!

}

void EncodeToRamYUYV(GLuint srcTexture, const TargetRectangle& sourceRc, u8* destAddr, int dstWidth, int dstHeight)
{
	g_renderer->ResetAPIState();

	s_rgbToYuyvProgram.Bind();

	glUniform4f(s_rgbToYuyvUniform_loc, static_cast<float>(sourceRc.left), static_cast<float>(sourceRc.top),
		static_cast<float>(sourceRc.right), static_cast<float>(sourceRc.bottom));

	// We enable linear filtering, because the gamecube does filtering in the vertical direction when
	// yscale is enabled.
	// Otherwise we get jaggies when a game uses yscaling (most PAL games)
	EncodeToRamUsingShader(srcTexture, destAddr, dstWidth / 2, dstHeight, dstWidth*dstHeight*2, true);
	FramebufferManager::SetFramebuffer(0);
	TextureCache::DisableStage(0);
	g_renderer->RestoreAPIState();
}


// Should be scale free.
void DecodeToTexture(u32 xfbAddr, int srcWidth, int srcHeight, GLuint destTexture)
{
	u8* srcAddr = Memory::GetPointer(xfbAddr);
	if (!srcAddr)
	{
		WARN_LOG(VIDEO, "Tried to decode from invalid memory address");
		return;
	}

	g_renderer->ResetAPIState(); // reset any game specific settings

	// switch to texture converter frame buffer
	// attach destTexture as color destination
	FramebufferManager::SetFramebuffer(s_texConvFrameBuffer[1]);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, destTexture, 0);

	// activate source texture
	// set srcAddr as data for source texture
	glActiveTexture(GL_TEXTURE0+9);
	glBindTexture(GL_TEXTURE_2D, s_srcTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, srcWidth / 2, srcHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, srcAddr);

	glViewport(0, 0, srcWidth, srcHeight);
	s_yuyvToRgbProgram.Bind();

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	FramebufferManager::SetFramebuffer(0);

	g_renderer->RestoreAPIState();
}

}  // namespace

}  // namespace OGL
