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
#include "PixelShaderCache.h"
#include "VertexShaderManager.h"
#include "VertexShaderCache.h"
#include "FramebufferManager.h"
#include "Globals.h"
#include "VideoConfig.h"
#include "ImageWrite.h"
#include "Render.h"
#include "D3DBase.h"
#include "D3DTexture.h"
#include "D3DUtil.h"
#include "D3DShader.h"
#include "TextureCache.h"

namespace TextureConverter
{

/*static LPDIRECT3DTEXTURE9 s_texConvFrameBuffer = 0;
static LPDIRECT3DTEXTURE9 s_srcTexture = 0;			// for decoding from RAM
static u32 s_srcTextureWidth = 0;
static u32 s_srcTextureHeight = 0;
static LPDIRECT3DSURFACE9  s_texConvReadSurface = 0;

const int renderBufferWidth = 1024;
const int renderBufferHeight = 1024;*/

static LPDIRECT3DPIXELSHADER9 s_rgbToYuyvProgram = NULL;
static LPDIRECT3DPIXELSHADER9 s_yuyvToRgbProgram = NULL;

// Not all slots are taken - but who cares.
const u32 NUM_ENCODING_PROGRAMS = 64;
static LPDIRECT3DPIXELSHADER9 s_encodingPrograms[NUM_ENCODING_PROGRAMS];

void CreateRgbToYuyvProgram()
{
	// Output is BGRA because that is slightly faster than RGBA.
	const char *FProgram =
	"uniform sampler samp0 : register(s0);\n"	
	"void main(\n"
	"  out float4 ocol0 : COLOR0,\n"
	"  in float2 uv0 : TEXCOORD0)\n"
	"{\n"		
	"  float2 uv1 = float2(uv0.x + 1.0f, uv0.y);\n"
	"  float3 c0 = tex2D(samp0, uv0).rgb;\n"
	"  float3 c1 = tex2D(samp0, uv1).rgb;\n"
	"  float3 y_const = float3(0.257f,0.504f,0.098f);\n"
	"  float3 u_const = float3(-0.148f,-0.291f,0.439f);\n"
	"  float3 v_const = float3(0.439f,-0.368f,-0.071f);\n"
	"  float4 const3 = float4(0.0625f,0.5f,0.0625f,0.5f);\n"
	"  float3 c01 = (c0 + c1) * 0.5f;\n"  
	"  ocol0 = float4(dot(c1,y_const),dot(c01,u_const),dot(c0,y_const),dot(c01, v_const)) + const3;\n"	  	
	"}\n";
	s_rgbToYuyvProgram = D3D::CompilePixelShader(FProgram, (int)strlen(FProgram));
	if (!s_rgbToYuyvProgram) {
        ERROR_LOG(VIDEO, "Failed to create RGB to YUYV fragment program");
    }
}

void CreateYuyvToRgbProgram()
{
	const char *FProgram =
	"uniform sampler samp0 : register(s0);\n"	
	"void main(\n"
	"  out float4 ocol0 : COLOR0,\n"
	"  in float2 uv0 : TEXCOORD0)\n"
	"{\n"		
	"  float4 c0 = tex2D(samp0, uv0).rgba;\n"

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
	s_yuyvToRgbProgram = D3D::CompilePixelShader(FProgram, (int)strlen(FProgram));
	if (!s_yuyvToRgbProgram) {
        ERROR_LOG(VIDEO, "Failed to create YUYV to RGB fragment program");
    }
}

LPDIRECT3DPIXELSHADER9 GetOrCreateEncodingShader(u32 format)
{
	if (format > NUM_ENCODING_PROGRAMS)
	{
		PanicAlert("Unknown texture copy format: 0x%x\n", format);
		return s_encodingPrograms[0];
	}

	if (!s_encodingPrograms[format])
	{
		const char* shader = TextureConversionShader::GenerateEncodingShader(format,true);

#if defined(_DEBUG) || defined(DEBUGFAST)
		if (g_ActiveConfig.iLog & CONF_SAVESHADERS && shader) {
			static int counter = 0;
			char szTemp[MAX_PATH];
			sprintf(szTemp, "%s/enc_%04i.txt", FULL_DUMP_DIR, counter++);

			SaveData(szTemp, shader);
		}
#endif
		s_encodingPrograms[format] = D3D::CompilePixelShader(shader, (int)strlen(shader));
		if (!s_encodingPrograms[format]) {
			ERROR_LOG(VIDEO, "Failed to create encoding fragment program");
		}
    }
	return s_encodingPrograms[format];
}

void Init()
{
	/*HRESULT hr = D3D::dev->CreateTexture(renderBufferWidth, renderBufferHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
		                                 D3DPOOL_DEFAULT, &s_texConvFrameBuffer, NULL);
	hr = D3D::dev->CreateTexture(renderBufferWidth, renderBufferHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
		                                 D3DPOOL_DEFAULT, &s_srcTexture, NULL);
	hr = D3D::dev->CreateOffscreenPlainSurface(renderBufferWidth, renderBufferWidth, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &s_texConvReadSurface, NULL );*/
	for (unsigned int i = 0; i < NUM_ENCODING_PROGRAMS; i++)
	{
		s_encodingPrograms[i] = NULL;
	}
	CreateRgbToYuyvProgram();
	CreateYuyvToRgbProgram();
}

void Shutdown()
{
	if(s_rgbToYuyvProgram)
		s_rgbToYuyvProgram->Release();
	s_rgbToYuyvProgram = NULL;
	if(s_yuyvToRgbProgram)
		s_yuyvToRgbProgram->Release();
	s_yuyvToRgbProgram=NULL;

	/*if(s_texConvReadSurface)
		s_texConvReadSurface->Release();
	s_texConvReadSurface=NULL;*/

	for (unsigned int i = 0; i < NUM_ENCODING_PROGRAMS; i++)
	{
		if(s_encodingPrograms[i]) 
			s_encodingPrograms[i]->Release();
		s_encodingPrograms[i] = NULL;
	}
	/*if(s_srcTexture)
		s_srcTexture->Release();
	s_srcTexture=NULL;
	if(s_texConvFrameBuffer)
		s_texConvFrameBuffer->Release();
	s_texConvFrameBuffer = NULL;*/
}

void EncodeToRamUsingShader(LPDIRECT3DPIXELSHADER9 shader, LPDIRECT3DTEXTURE9 srcTexture,int srcTextureWidth,int srcTextureHeight, const TargetRectangle& sourceRc,
				            u8* destAddr, int dstWidth, int dstHeight, int readStride, bool toTexture, bool linearFilter)
{
	HRESULT hr;
	Renderer::ResetAPIState();	
	// switch to texture converter frame buffer	
	LPDIRECT3DTEXTURE9 tempTexture = 0;
	hr = D3D::dev->CreateTexture(dstWidth, dstHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
		                                 D3DPOOL_DEFAULT, &tempTexture, NULL);
	LPDIRECT3DSURFACE9 Rendersurf = NULL;

	tempTexture->GetSurfaceLevel(0,&Rendersurf);
	hr = D3D::dev->SetDepthStencilSurface(NULL);
	hr = D3D::dev->SetRenderTarget(0, Rendersurf);
	
	
	if (linearFilter)
	{
		D3D::dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		D3D::dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		D3D::dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
	}
	else
	{
		D3D::dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		D3D::dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		D3D::dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
	}

	
	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = dstWidth;
	vp.Height = dstHeight;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	hr = D3D::dev->SetViewport(&vp);	
	RECT SrcRect;
	SrcRect.top = sourceRc.top;
	SrcRect.left = sourceRc.left;
	SrcRect.right = sourceRc.right;
	SrcRect.bottom = sourceRc.bottom;
	RECT DstRect;
	DstRect.top = 0;
	DstRect.left = 0;
	DstRect.right = dstWidth;
	DstRect.bottom = dstHeight;


	// Draw...
	D3D::drawShadedTexQuad(srcTexture,&SrcRect,srcTextureWidth,srcTextureHeight,&DstRect,shader,VertexShaderCache::GetSimpleVertexShader());
	hr = D3D::dev->SetRenderTarget(0, FBManager::GetEFBColorRTSurface());
	hr = D3D::dev->SetDepthStencilSurface(FBManager::GetEFBDepthRTSurface());
	Renderer::RestoreAPIState();	
	D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);
	D3D::RefreshSamplerState(0, D3DSAMP_MAGFILTER);
	D3D::RefreshSamplerState(0, D3DSAMP_MIPFILTER);

	// .. and then readback the results.
	// TODO: make this less slow.

	D3DLOCKED_RECT drect;
	
	LPDIRECT3DSURFACE9  s_tempConvReadSurface = 0;
	hr = D3D::dev->CreateOffscreenPlainSurface(dstWidth, dstHeight, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &s_tempConvReadSurface, NULL );
	hr = D3D::dev->GetRenderTargetData(Rendersurf,s_tempConvReadSurface);
	if((hr = s_tempConvReadSurface->LockRect(&drect, &DstRect, D3DLOCK_READONLY)) != D3D_OK)
	{
		PanicAlert("ERROR: %s", hr == D3DERR_WASSTILLDRAWING ? "Still drawing" :
											  hr == D3DERR_INVALIDCALL     ? "Invalid call" : "w00t");	
		
	}
	else
	{
		int writeStride = bpmem.copyMipMapStrideChannels * 32;

		if (writeStride != readStride && toTexture)
		{
			// writing to a texture of a different size

			int readHeight = readStride / dstWidth;
			//readHeight /= 4; // 4 bytes per pixel

			int readStart = 0;
			int readLoops = dstHeight / (readHeight/4);
			u8 *Source = (u8*)drect.pBits;
			for (int i = 0; i < readLoops; i++)
			{
				memcpy(destAddr,Source,dstWidth*readHeight);
				Source += readHeight;
				destAddr += writeStride;
			}
		}
		else
			memcpy(destAddr,drect.pBits,dstWidth*dstHeight*4);
		
		hr = s_tempConvReadSurface->UnlockRect();
	}
	s_tempConvReadSurface->Release();		
	Rendersurf->Release();
	tempTexture->Release();	
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

	LPDIRECT3DPIXELSHADER9 texconv_shader = GetOrCreateEncodingShader(format);
	if (!texconv_shader)
		return;

	u8 *dest_ptr = Memory_GetPtr(address);

	LPDIRECT3DTEXTURE9 source_texture = bFromZBuffer ? FBManager::GetEFBDepthTexture(source) : FBManager::GetEFBColorTexture(source);
	int width = (source.right - source.left) >> bScaleByHalf;
	int height = (source.bottom - source.top) >> bScaleByHalf;

	int size_in_bytes = TexDecoder_GetTextureSizeInBytes(width, height, format);

	// Invalidate any existing texture covering this memory range.
	// TODO - don't delete the texture if it already exists, just replace the contents.
	TextureCache::InvalidateRange(address, size_in_bytes);
	
	u16 blkW = TexDecoder_GetBlockWidthInTexels(format) - 1;
	u16 blkH = TexDecoder_GetBlockHeightInTexels(format) - 1;	
	u16 samples = TextureConversionShader::GetEncodedSampleCount(format);	

	// only copy on cache line boundaries
	// extra pixels are copied but not displayed in the resulting texture
	s32 expandedWidth = (width + blkW) & (~blkW);
	s32 expandedHeight = (height + blkH) & (~blkH);

    float MValueX = Renderer::GetTargetScaleX();
	float MValueY = Renderer::GetTargetScaleY();

	float top = source.top * MValueY;
	float sampleStride = bScaleByHalf?2.0f:1.0f;

	TextureConversionShader::SetShaderParameters(
		(float)expandedWidth, 
		expandedHeight * MValueY, 
		source.left * MValueX, 
		top, 
		sampleStride * MValueX, 
		sampleStride * MValueY,
		(float)Renderer::GetTargetWidth(),
		(float)Renderer::GetTargetHeight());

	TargetRectangle scaledSource;
	scaledSource.top = 0;
	scaledSource.bottom = expandedHeight;
	scaledSource.left = 0;
	scaledSource.right = expandedWidth / samples;
	int cacheBytes = 32;
    if ((format & 0x0f) == 6)
        cacheBytes = 64;

    int readStride = (expandedWidth * cacheBytes) / TexDecoder_GetBlockWidthInTexels(format);
	EncodeToRamUsingShader(texconv_shader, source_texture,1,1, scaledSource, dest_ptr, expandedWidth / samples, expandedHeight,readStride, true, bScaleByHalf > 0);
}

/*void EncodeToRamYUYV(GLuint srcTexture, const TargetRectangle& sourceRc,
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
*/
}  // namespace
