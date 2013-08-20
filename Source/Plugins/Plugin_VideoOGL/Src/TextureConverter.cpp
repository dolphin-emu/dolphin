// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// Fast image conversion using OpenGL shaders.
// This kind of stuff would be a LOT nicer with OpenCL.

#include <math.h>

#include "TextureConverter.h"
#include "TextureConversionShader.h"
#include "TextureCache.h"
#include "ProgramShaderCache.h"
#include "VertexShaderManager.h"
#include "FramebufferManager.h"
#include "Globals.h"
#include "VideoConfig.h"
#include "ImageWrite.h"
#include "Render.h"
#include "FileUtil.h"
#include "HW/Memmap.h"

namespace OGL
{

namespace TextureConverter
{

using OGL::TextureCache;

static GLuint s_texConvFrameBuffer = 0;
static GLuint s_srcTexture = 0;			// for decoding from RAM
static GLuint s_srcTextureWidth = 0;
static GLuint s_srcTextureHeight = 0;
static GLuint s_dstTexture = 0;		// for encoding to RAM

const int renderBufferWidth = 1024;
const int renderBufferHeight = 1024;

static SHADER s_rgbToYuyvProgram;
static SHADER s_yuyvToRgbProgram;

// Not all slots are taken - but who cares.
const u32 NUM_ENCODING_PROGRAMS = 64;
static SHADER s_encodingPrograms[NUM_ENCODING_PROGRAMS];

static GLuint s_encode_VBO = 0;
static GLuint s_encode_VAO = 0;
static GLuint s_decode_VBO = 0;
static GLuint s_decode_VAO = 0;
static TargetRectangle s_cached_sourceRc;
static int s_cached_srcWidth = 0;
static int s_cached_srcHeight = 0;

static const char *VProgram =
	"ATTRIN vec2 rawpos;\n"
	"ATTRIN vec2 tex0;\n"
	"VARYOUT vec2 uv0;\n"
	"void main()\n"
	"{\n"
	"	uv0 = tex0;\n"
	"	gl_Position = vec4(rawpos, 0.0f, 1.0f);\n"
	"}\n";

void CreatePrograms()
{
	// Output is BGRA because that is slightly faster than RGBA.
	const char *FProgramRgbToYuyv =
		"uniform sampler2DRect samp9;\n"
		"VARYIN vec2 uv0;\n"
		"COLOROUT(ocol0)\n"
		"void main()\n"
		"{\n"
		"	vec3 c0 = texture2DRect(samp9, uv0).rgb;\n"
		"	vec3 c1 = texture2DRect(samp9, uv0 + vec2(1.0, 0.0)).rgb;\n"
		"	vec3 c01 = (c0 + c1) * 0.5;\n"
		"	vec3 y_const = vec3(0.257,0.504,0.098);\n"
		"	vec3 u_const = vec3(-0.148,-0.291,0.439);\n"
		"	vec3 v_const = vec3(0.439,-0.368,-0.071);\n"
		"	vec4 const3 = vec4(0.0625,0.5,0.0625f,0.5);\n"
		"	ocol0 = vec4(dot(c1,y_const),dot(c01,u_const),dot(c0,y_const),dot(c01, v_const)) + const3;\n"
		"}\n";

	const char *FProgramYuyvToRgb =
		"uniform sampler2DRect samp9;\n"
		"VARYIN vec2 uv0;\n"
		"COLOROUT(ocol0)\n"
		"void main()\n"
		"{\n"
		"	vec4 c0 = texture2DRect(samp9, uv0).rgba;\n"
		"	float f = step(0.5, fract(uv0.x));\n"
		"	float y = mix(c0.b, c0.r, f);\n"
		"	float yComp = 1.164f * (y - 0.0625f);\n"
		"	float uComp = c0.g - 0.5f;\n"
		"	float vComp = c0.a - 0.5f;\n"
		"	ocol0 = vec4(yComp + (1.596f * vComp),\n"
		"		yComp - (0.813f * vComp) - (0.391f * uComp),\n"
		"		yComp + (2.018f * uComp),\n"
		"		1.0f);\n"
		"}\n";

	ProgramShaderCache::CompileShader(s_rgbToYuyvProgram, VProgram, FProgramRgbToYuyv);
	ProgramShaderCache::CompileShader(s_yuyvToRgbProgram, VProgram, FProgramYuyvToRgb);
}

SHADER &GetOrCreateEncodingShader(u32 format)
{
	if (format > NUM_ENCODING_PROGRAMS)
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
			char szTemp[MAX_PATH];
			sprintf(szTemp, "%senc_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), counter++);

			SaveData(szTemp, shader);
		}
#endif

		ProgramShaderCache::CompileShader(s_encodingPrograms[format], VProgram, shader);
	}
	return s_encodingPrograms[format];
}

void Init()
{
	glGenFramebuffers(1, &s_texConvFrameBuffer);

	glGenBuffers(1, &s_encode_VBO );
	glGenVertexArrays(1, &s_encode_VAO );
	glBindBuffer(GL_ARRAY_BUFFER, s_encode_VBO );
	glBindVertexArray( s_encode_VAO );
	glEnableVertexAttribArray(SHADER_POSITION_ATTRIB);
	glVertexAttribPointer(SHADER_POSITION_ATTRIB, 2, GL_FLOAT, 0, sizeof(GLfloat)*4, (GLfloat*)NULL);
	glEnableVertexAttribArray(SHADER_TEXTURE0_ATTRIB);
	glVertexAttribPointer(SHADER_TEXTURE0_ATTRIB, 2, GL_FLOAT, 0, sizeof(GLfloat)*4, (GLfloat*)NULL+2);
	s_cached_sourceRc.top = -1;
	s_cached_sourceRc.bottom = -1;
	s_cached_sourceRc.left = -1;
	s_cached_sourceRc.right = -1;

	glGenBuffers(1, &s_decode_VBO );
	glGenVertexArrays(1, &s_decode_VAO );
	glBindBuffer(GL_ARRAY_BUFFER, s_decode_VBO );
	glBindVertexArray( s_decode_VAO );
	s_cached_srcWidth = -1;
	s_cached_srcHeight = -1;
	glEnableVertexAttribArray(SHADER_POSITION_ATTRIB);
	glVertexAttribPointer(SHADER_POSITION_ATTRIB, 2, GL_FLOAT, 0, sizeof(GLfloat)*4, (GLfloat*)NULL);
	glEnableVertexAttribArray(SHADER_TEXTURE0_ATTRIB);
	glVertexAttribPointer(SHADER_TEXTURE0_ATTRIB, 2, GL_FLOAT, 0, sizeof(GLfloat)*4, (GLfloat*)NULL+2);

	s_srcTextureWidth = 0;
	s_srcTextureHeight = 0;

	glActiveTexture(GL_TEXTURE0 + 9);
	glGenTextures(1, &s_srcTexture);
	glBindTexture(getFbType(), s_srcTexture);
	glTexParameteri(getFbType(), GL_TEXTURE_MAX_LEVEL, 0);

	glGenTextures(1, &s_dstTexture);
	glBindTexture(GL_TEXTURE_2D, s_dstTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, renderBufferWidth, renderBufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	
	CreatePrograms();
}

void Shutdown()
{
	glDeleteTextures(1, &s_srcTexture);
	glDeleteTextures(1, &s_dstTexture);
	glDeleteFramebuffers(1, &s_texConvFrameBuffer);
	glDeleteBuffers(1, &s_encode_VBO );
	glDeleteVertexArrays(1, &s_encode_VAO );
	glDeleteBuffers(1, &s_decode_VBO );
	glDeleteVertexArrays(1, &s_decode_VAO );

	s_rgbToYuyvProgram.Destroy();
	s_yuyvToRgbProgram.Destroy();

	for (unsigned int i = 0; i < NUM_ENCODING_PROGRAMS; i++)
		s_encodingPrograms[i].Destroy();

	s_srcTexture = 0;
	s_dstTexture = 0;
	s_texConvFrameBuffer = 0;
}

void EncodeToRamUsingShader(GLuint srcTexture, const TargetRectangle& sourceRc,
						u8* destAddr, int dstWidth, int dstHeight, int readStride,
							bool toTexture, bool linearFilter)
{


	// switch to texture converter frame buffer
	// attach render buffer as color destination
	FramebufferManager::SetFramebuffer(s_texConvFrameBuffer);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_dstTexture, 0);
	GL_REPORT_ERRORD();

	// set source texture
	glActiveTexture(GL_TEXTURE0+9);
	glBindTexture(getFbType(), srcTexture);

	if (linearFilter)
	{
		glTexParameteri(getFbType(), GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(getFbType(), GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	else
	{
		glTexParameteri(getFbType(), GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(getFbType(), GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}

	GL_REPORT_ERRORD();

	glViewport(0, 0, (GLsizei)dstWidth, (GLsizei)dstHeight);

	GL_REPORT_ERRORD();
	if(!(s_cached_sourceRc == sourceRc)) {
		GLfloat vertices[] = {
			-1.f, -1.f, 
			(float)sourceRc.left, (float)sourceRc.top,
			-1.f, 1.f,
			(float)sourceRc.left, (float)sourceRc.bottom,
			1.f, -1.f,
			(float)sourceRc.right, (float)sourceRc.top,
			1.f, 1.f,
			(float)sourceRc.right, (float)sourceRc.bottom
		};
		glBindBuffer(GL_ARRAY_BUFFER, s_encode_VBO );
		glBufferData(GL_ARRAY_BUFFER, 4*4*sizeof(GLfloat), vertices, GL_STREAM_DRAW);
		
		s_cached_sourceRc = sourceRc;
	} 

	glBindVertexArray( s_encode_VAO );
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	
	GL_REPORT_ERRORD();

	// .. and then read back the results.
	// TODO: make this less slow.

	int writeStride = bpmem.copyMipMapStrideChannels * 32;

	if (writeStride != readStride && toTexture)
	{
		// writing to a texture of a different size

		int readHeight = readStride / dstWidth;
		readHeight /= 4; // 4 bytes per pixel

		int readStart = 0;
		int readLoops = dstHeight / readHeight;
		for (int i = 0; i < readLoops; i++)
		{
			glReadPixels(0, readStart, (GLsizei)dstWidth, (GLsizei)readHeight, GL_BGRA, GL_UNSIGNED_BYTE, destAddr);
			readStart += readHeight;
			destAddr += writeStride;
		}
	}
	else
		glReadPixels(0, 0, (GLsizei)dstWidth, (GLsizei)dstHeight, GL_BGRA, GL_UNSIGNED_BYTE, destAddr);

	GL_REPORT_ERRORD();

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
		if (copyfmt > GX_TF_RGBA8 || (copyfmt < GX_TF_RGB565 && !bIsIntensityFmt))
			format |= _GX_TF_CTF;

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
		
	float sampleStride = bScaleByHalf ? 2.f : 1.f;
	
	float params[] = {
		Renderer::EFBToScaledXf(sampleStride), Renderer::EFBToScaledYf(sampleStride),
		0.0f, 0.0f,
		(float)expandedWidth, (float)Renderer::EFBToScaledY(expandedHeight)-1,
		(float)Renderer::EFBToScaledX(source.left), (float)Renderer::EFBToScaledY(EFB_HEIGHT - source.top - expandedHeight)
	};

	texconv_shader.Bind();
	glUniform4fv(texconv_shader.UniformLocations[C_COLORS], 2, params);

	TargetRectangle scaledSource;
	scaledSource.top = 0;
	scaledSource.bottom = expandedHeight;
	scaledSource.left = 0;
	scaledSource.right = expandedWidth / samples;
	int cacheBytes = 32;
	if ((format & 0x0f) == 6)
		cacheBytes = 64;

	int readStride = (expandedWidth * cacheBytes) /
		TexDecoder_GetBlockWidthInTexels(format);
	EncodeToRamUsingShader(source_texture, scaledSource,
		dest_ptr, expandedWidth / samples, expandedHeight, readStride,
		true, bScaleByHalf > 0 && !bFromZBuffer);
	return size_in_bytes; // TODO: D3D11 is calculating this value differently!

}

void EncodeToRamYUYV(GLuint srcTexture, const TargetRectangle& sourceRc, u8* destAddr, int dstWidth, int dstHeight)
{
	g_renderer->ResetAPIState();
	
	s_rgbToYuyvProgram.Bind();
		
	EncodeToRamUsingShader(srcTexture, sourceRc, destAddr, dstWidth / 2, dstHeight, 0, false, false);
	FramebufferManager::SetFramebuffer(0);
	VertexShaderManager::SetViewportChanged();
	TextureCache::DisableStage(0);
	g_renderer->RestoreAPIState();
	GL_REPORT_ERRORD();
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

	int srcFmtWidth = srcWidth / 2;

	g_renderer->ResetAPIState(); // reset any game specific settings

	// switch to texture converter frame buffer
	// attach destTexture as color destination
	FramebufferManager::SetFramebuffer(s_texConvFrameBuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, destTexture, 0);

	GL_REPORT_FBO_ERROR();

	// activate source texture
	// set srcAddr as data for source texture
	glActiveTexture(GL_TEXTURE0+9);
	glBindTexture(getFbType(), s_srcTexture);

	// TODO: make this less slow.  (How?)
	if ((GLsizei)s_srcTextureWidth == (GLsizei)srcFmtWidth && (GLsizei)s_srcTextureHeight == (GLsizei)srcHeight)
	{
		glTexSubImage2D(getFbType(), 0,0,0,s_srcTextureWidth, s_srcTextureHeight,
				GL_BGRA, GL_UNSIGNED_BYTE, srcAddr);
	}
	else
	{
		glTexImage2D(getFbType(), 0, GL_RGBA8, (GLsizei)srcFmtWidth, (GLsizei)srcHeight,
				0, GL_BGRA, GL_UNSIGNED_BYTE, srcAddr);
		s_srcTextureWidth = (GLsizei)srcFmtWidth;
		s_srcTextureHeight = (GLsizei)srcHeight;
	}

	glViewport(0, 0, srcWidth, srcHeight);
	s_yuyvToRgbProgram.Bind();

	GL_REPORT_ERRORD();
	
	if(s_cached_srcHeight != srcHeight || s_cached_srcWidth != srcWidth) {
		GLfloat vertices[] = {
			1.f, -1.f,
			(float)srcFmtWidth, (float)srcHeight,
			1.f, 1.f,
			(float)srcFmtWidth, 0.f,
			-1.f, -1.f,
			0.f, (float)srcHeight,
			-1.f, 1.f,
			0.f, 0.f
		};
		
		glBindBuffer(GL_ARRAY_BUFFER, s_decode_VBO );
		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*4*4, vertices, GL_STREAM_DRAW);
		
		s_cached_srcHeight = srcHeight;
		s_cached_srcWidth = srcWidth;
	}
	
	glBindVertexArray( s_decode_VAO );
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	
	GL_REPORT_ERRORD();

	VertexShaderManager::SetViewportChanged();

	FramebufferManager::SetFramebuffer(0);

	g_renderer->RestoreAPIState();
	GL_REPORT_ERRORD();
}

}  // namespace

}  // namespace OGL
