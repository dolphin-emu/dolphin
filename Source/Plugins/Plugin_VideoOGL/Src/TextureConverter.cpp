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

// Fast image conversion using OpenGL shaders.
// This kind of stuff would be a LOT nicer with OpenCL.

#include "TextureConverter.h"
#include "TextureConversionShader.h"
#include "TextureMngr.h"
#include "PixelShaderCache.h"
#include "VertexShaderManager.h"
#include "FramebufferManager.h"
#include "Globals.h"
#include "VideoConfig.h"
#include "ImageWrite.h"
#include "Render.h"

namespace TextureConverter
{

static GLuint s_texConvFrameBuffer = 0;
static GLuint s_srcTexture = 0;			// for decoding from RAM
static GLuint s_srcTextureWidth = 0;
static GLuint s_srcTextureHeight = 0;
static GLuint s_dstRenderBuffer = 0;	// for encoding to RAM

const int renderBufferWidth = 1024;
const int renderBufferHeight = 1024;

static FRAGMENTSHADER s_rgbToYuyvProgram;
static FRAGMENTSHADER s_yuyvToRgbProgram;

// Not all slots are taken - but who cares.
const u32 NUM_ENCODING_PROGRAMS = 64;
static FRAGMENTSHADER s_encodingPrograms[NUM_ENCODING_PROGRAMS];

void CreateRgbToYuyvProgram()
{
	// Output is BGRA because that is slightly faster than RGBA.
	const char *FProgram =
	"uniform samplerRECT samp0 : register(s0);\n"	
	"void main(\n"
	"  out float4 ocol0 : COLOR0,\n"
	"  in float2 uv0 : TEXCOORD0)\n"
	"{\n"		
	"  float2 uv1 = float2(uv0.x + 1.0f, uv0.y);\n"
	"  float3 c0 = texRECT(samp0, uv0).rgb;\n"
	"  float3 c1 = texRECT(samp0, uv1).rgb;\n"
	"  float3 y_const = float3(0.257f,0.504f,0.098f);\n"
	"  float3 u_const = float3(-0.148f,-0.291f,0.439f);\n"
	"  float3 v_const = float3(0.439f,-0.368f,-0.071f);\n"
	"  float4 const3 = float4(0.0625f,0.5f,0.0625f,0.5f);\n"
	"  float3 c01 = (c0 + c1) * 0.5f;\n"  
	"  ocol0 = float4(dot(c1,y_const),dot(c01,u_const),dot(c0,y_const),dot(c01, v_const)) + const3;\n"	  	
	"}\n";

	if (!PixelShaderCache::CompilePixelShader(s_rgbToYuyvProgram, FProgram)) {
        ERROR_LOG(VIDEO, "Failed to create RGB to YUYV fragment program");
    }
}

void CreateYuyvToRgbProgram()
{
	const char *FProgram =
	"uniform samplerRECT samp0 : register(s0);\n"	
	"void main(\n"
	"  out float4 ocol0 : COLOR0,\n"
	"  in float2 uv0 : TEXCOORD0)\n"
	"{\n"		
	"  float4 c0 = texRECT(samp0, uv0).rgba;\n"

	"  float f = step(0.5, frac(uv0.x));\n"
	"  float y = lerp(c0.b, c0.r, f);\n"
	"  float yComp = 1.164f * (y - 0.0625f);\n"
	"  float uComp = c0.g - 0.5f;\n"
	"  float vComp = c0.a - 0.5f;\n"

    "  ocol0 = float4(yComp + (1.596f * vComp),\n"
	"                 yComp - (0.813f * vComp) - (0.391f * uComp),\n"
	"                 yComp + (2.018f * uComp),\n"
	"                 1.0f);\n"
	"}\n";

	if (!PixelShaderCache::CompilePixelShader(s_yuyvToRgbProgram, FProgram)) {
        ERROR_LOG(VIDEO, "Failed to create YUYV to RGB fragment program");
    }
}

FRAGMENTSHADER &GetOrCreateEncodingShader(u32 format)
{
	if (format > NUM_ENCODING_PROGRAMS)
	{
		PanicAlert("Unknown texture copy format: 0x%x\n", format);
		return s_encodingPrograms[0];
	}

	if (s_encodingPrograms[format].glprogid == 0)
	{
		const char* shader = TextureConversionShader::GenerateEncodingShader(format);

#if defined(_DEBUG) || defined(DEBUGFAST)
		if (g_ActiveConfig.iLog & CONF_SAVESHADERS && shader) {
			static int counter = 0;
			char szTemp[MAX_PATH];
			sprintf(szTemp, "%s/enc_%04i.txt", FULL_DUMP_DIR, counter++);

			SaveData(szTemp, shader);
		}
#endif

		if (!PixelShaderCache::CompilePixelShader(s_encodingPrograms[format], shader)) {
			const char* error = cgGetLastListing(g_cgcontext);
			ERROR_LOG(VIDEO, "Failed to create encoding fragment program:\n%s", error);
		}
    }

	return s_encodingPrograms[format];
}

void Init()
{
	glGenFramebuffersEXT(1, &s_texConvFrameBuffer);

	glGenRenderbuffersEXT(1, &s_dstRenderBuffer);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, s_dstRenderBuffer);
	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA, renderBufferWidth, renderBufferHeight);

	glGenTextures(1, &s_srcTexture);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, s_srcTexture);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	CreateRgbToYuyvProgram();
	CreateYuyvToRgbProgram();
}

void Shutdown()
{
	glDeleteTextures(1, &s_srcTexture);	
	glDeleteRenderbuffersEXT(1, &s_dstRenderBuffer);
	glDeleteFramebuffersEXT(1, &s_texConvFrameBuffer);

	s_rgbToYuyvProgram.Destroy();
	s_yuyvToRgbProgram.Destroy();

	for (unsigned int i = 0; i < NUM_ENCODING_PROGRAMS; i++)
		s_encodingPrograms[i].Destroy();

	s_srcTexture = 0;
	s_dstRenderBuffer = 0;
	s_texConvFrameBuffer = 0;
}

void EncodeToRamUsingShader(FRAGMENTSHADER& shader, GLuint srcTexture, const TargetRectangle& sourceRc,
				            u8* destAddr, int dstWidth, int dstHeight, int readStride, bool toTexture, bool linearFilter)
{
	Renderer::ResetAPIState();
	
	// switch to texture converter frame buffer
	// attach render buffer as color destination
	g_framebufferManager.SetFramebuffer(s_texConvFrameBuffer);

	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, s_dstRenderBuffer);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, s_dstRenderBuffer);	
	GL_REPORT_ERRORD();
	
	for (int i = 1; i < 8; ++i)
		TextureMngr::DisableStage(i);

	// set source texture
	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_RECTANGLE_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, srcTexture);

	if (linearFilter)
	{
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}

	GL_REPORT_ERRORD();

	glViewport(0, 0, (GLsizei)dstWidth, (GLsizei)dstHeight);

	PixelShaderCache::EnableShader(shader.glprogid);

	// Draw...
	glBegin(GL_QUADS);
    glTexCoord2f((float)sourceRc.left, (float)sourceRc.top);     glVertex2f(-1,-1);
	glTexCoord2f((float)sourceRc.left, (float)sourceRc.bottom);  glVertex2f(-1,1);
    glTexCoord2f((float)sourceRc.right, (float)sourceRc.bottom); glVertex2f(1,1);
    glTexCoord2f((float)sourceRc.right, (float)sourceRc.top);    glVertex2f(1,-1);
    glEnd();
	GL_REPORT_ERRORD();

	// .. and then readback the results.
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

	g_framebufferManager.SetFramebuffer(0);
    VertexShaderManager::SetViewportChanged();	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    TextureMngr::DisableStage(0);
	Renderer::RestoreAPIState();
    GL_REPORT_ERRORD();
}

void EncodeToRam(u32 address, bool bFromZBuffer, bool bIsIntensityFmt, u32 copyfmt, int bScaleByHalf, const EFBRectangle& source)
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

	FRAGMENTSHADER& texconv_shader = GetOrCreateEncodingShader(format);
	if (texconv_shader.glprogid == 0)
		return;

	u8 *dest_ptr = Memory_GetPtr(address);

	GLuint source_texture = bFromZBuffer ? g_framebufferManager.ResolveAndGetDepthTarget(source) : g_framebufferManager.ResolveAndGetRenderTarget(source);

	int width = (source.right - source.left) >> bScaleByHalf;
	int height = (source.bottom - source.top) >> bScaleByHalf;

	int size_in_bytes = TexDecoder_GetTextureSizeInBytes(width, height, format);

	// Invalidate any existing texture covering this memory range.
	// TODO - don't delete the texture if it already exists, just replace the contents.
	TextureMngr::InvalidateRange(address, size_in_bytes);
	
	u16 blkW = TexDecoder_GetBlockWidthInTexels(format) - 1;
	u16 blkH = TexDecoder_GetBlockHeightInTexels(format) - 1;	
	u16 samples = TextureConversionShader::GetEncodedSampleCount(format);	

	// only copy on cache line boundaries
	// extra pixels are copied but not displayed in the resulting texture
	s32 expandedWidth = (width + blkW) & (~blkW);
	s32 expandedHeight = (height + blkH) & (~blkH);

    float MValueX = Renderer::GetTargetScaleX();
	float MValueY = Renderer::GetTargetScaleY();

	float top = Renderer::GetTargetHeight() - floorf((source.top + expandedHeight) * MValueY - 0.5f);

    float sampleStride = bScaleByHalf?2.0f:1.0f;

	TextureConversionShader::SetShaderParameters((float)expandedWidth, 
		expandedHeight * MValueY, 
		ceilf(source.left * MValueX + 0.5f), 
		top, 
		sampleStride * MValueX, 
		sampleStride * MValueY);

	TargetRectangle scaledSource;
	scaledSource.top = 0;
	scaledSource.bottom = expandedHeight;
	scaledSource.left = 0;
	scaledSource.right = expandedWidth / samples;


    int cacheBytes = 32;
    if ((format & 0x0f) == 6)
        cacheBytes = 64;

    int readStride = (expandedWidth * cacheBytes) / TexDecoder_GetBlockWidthInTexels(format);

	EncodeToRamUsingShader(texconv_shader, source_texture, scaledSource, dest_ptr, expandedWidth / samples, expandedHeight, readStride, true, bScaleByHalf > 0);
}

void EncodeToRamYUYV(GLuint srcTexture, const TargetRectangle& sourceRc,
				     u8* destAddr, int dstWidth, int dstHeight)
{
	EncodeToRamUsingShader(s_rgbToYuyvProgram, srcTexture, sourceRc, destAddr, dstWidth / 2, dstHeight, 0, false, false);
}


// Should be scale free.
void DecodeToTexture(u32 xfbAddr, int srcWidth, int srcHeight, GLuint destTexture)
{
	u8* srcAddr = Memory_GetPtr(xfbAddr);
	if (!srcAddr)
	{
		WARN_LOG(VIDEO, "Tried to decode from invalid memory address");
		return;
	}

	Renderer::ResetAPIState();

	float srcFormatFactor = 0.5f;
	float srcFmtWidth = srcWidth * srcFormatFactor;

	// swich to texture converter frame buffer
	// attach destTexture as color destination
	g_framebufferManager.SetFramebuffer(s_texConvFrameBuffer);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, destTexture);	
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, destTexture, 0);

	GL_REPORT_FBO_ERROR();

    for (int i = 1; i < 8; ++i)
		TextureMngr::DisableStage(i);

	// activate source texture
	// set srcAddr as data for source texture
	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_RECTANGLE_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, s_srcTexture);

	// TODO: make this less slow.  (How?)
	if(s_srcTextureWidth == (GLsizei)srcFmtWidth && s_srcTextureHeight == (GLsizei)srcHeight)
	{
		glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0,0,0,s_srcTextureWidth, s_srcTextureHeight, GL_BGRA, GL_UNSIGNED_BYTE, srcAddr);	
	}
	else
	{	
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8, (GLsizei)srcFmtWidth, (GLsizei)srcHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, srcAddr);	
		s_srcTextureWidth = (GLsizei)srcFmtWidth;
		s_srcTextureHeight = (GLsizei)srcHeight;
	}

	glViewport(0, 0, srcWidth, srcHeight);

	PixelShaderCache::EnableShader(s_yuyvToRgbProgram.glprogid);
	
	GL_REPORT_ERRORD();

    glBegin(GL_QUADS);
	glTexCoord2f(srcFmtWidth, (float)srcHeight); glVertex2f(1,-1);
	glTexCoord2f(srcFmtWidth, 0); glVertex2f(1,1);
	glTexCoord2f(0, 0); glVertex2f(-1,1);
	glTexCoord2f(0, (float)srcHeight); glVertex2f(-1,-1);
    glEnd();	

	// reset state
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, 0, 0);
    TextureMngr::DisableStage(0);

	VertexShaderManager::SetViewportChanged();

	g_framebufferManager.SetFramebuffer(0);

	Renderer::RestoreAPIState();
    GL_REPORT_ERRORD();
}

}  // namespace
