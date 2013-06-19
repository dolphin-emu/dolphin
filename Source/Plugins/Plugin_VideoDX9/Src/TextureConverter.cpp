// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
#include "TextureCache.h"
#include "Math.h"
#include "FileUtil.h"
#include "HW/Memmap.h"

namespace DX9
{

namespace TextureConverter
{
struct TransformBuffer
{
	LPDIRECT3DTEXTURE9 FBTexture;
	LPDIRECT3DSURFACE9 RenderSurface;
	LPDIRECT3DSURFACE9 ReadSurface;
	int Width;
	int Height;
};
const u32 NUM_TRANSFORM_BUFFERS = 16;
static TransformBuffer TrnBuffers[NUM_TRANSFORM_BUFFERS];
static u32 WorkingBuffers = 0;

static LPDIRECT3DPIXELSHADER9 s_rgbToYuyvProgram = NULL;
static LPDIRECT3DPIXELSHADER9 s_yuyvToRgbProgram = NULL;

// Not all slots are taken - but who cares.
const u32 NUM_ENCODING_PROGRAMS = 64;
static LPDIRECT3DPIXELSHADER9 s_encodingPrograms[NUM_ENCODING_PROGRAMS];
static bool s_encodingProgramsFailed[NUM_ENCODING_PROGRAMS];

void CreateRgbToYuyvProgram()
{
	// Output is BGRA because that is slightly faster than RGBA.
	char* FProgram = new char[2048];
	sprintf(FProgram,"uniform float4 blkDims : register(c%d);\n"
	"uniform float4 textureDims : register(c%d);\n"
	"uniform sampler samp0 : register(s0);\n"	
	"void main(\n"
	"  out float4 ocol0 : COLOR0,\n"
	"  in float2 uv0 : TEXCOORD0,\n"
	"  in float uv2 : TEXCOORD1)\n"
	"{\n"		
	"  float2 uv1 = float2((uv0.x + 1.0f)/ blkDims.z, uv0.y / blkDims.w);\n"
	"  float3 c0 = tex2D(samp0, uv0.xy / blkDims.zw).rgb;\n"
	"  float3 c1 = tex2D(samp0, uv1).rgb;\n"
	"  c0 = pow(c0,uv2.xxx);\n"
	"  c1 = pow(c1,uv2.xxx);\n"
	"  float3 y_const = float3(0.257f,0.504f,0.098f);\n"
	"  float3 u_const = float3(-0.148f,-0.291f,0.439f);\n"
	"  float3 v_const = float3(0.439f,-0.368f,-0.071f);\n"
	"  float4 const3 = float4(0.0625f,0.5f,0.0625f,0.5f);\n"
	"  float3 c01 = (c0 + c1) * 0.5f;\n"  
	"  ocol0 = float4(dot(c1,y_const),dot(c01,u_const),dot(c0,y_const),dot(c01, v_const)) + const3;\n"	  	
	"}\n",C_COLORMATRIX,C_COLORMATRIX+1);

	s_rgbToYuyvProgram = D3D::CompileAndCreatePixelShader(FProgram, (int)strlen(FProgram));
	if (!s_rgbToYuyvProgram) {
		ERROR_LOG(VIDEO, "Failed to create RGB to YUYV fragment program");
	}
	delete [] FProgram;
}

void CreateYuyvToRgbProgram()
{
	char* FProgram = new char[2048];
	sprintf(FProgram,"uniform float4 blkDims : register(c%d);\n"
	"uniform float4 textureDims : register(c%d);\n"
	"uniform sampler samp0 : register(s0);\n"	
	"void main(\n"
	"  out float4 ocol0 : COLOR0,\n"
	"  in float2 uv0 : TEXCOORD0)\n"
	"{\n"		
	"  float4 c0 = tex2D(samp0, uv0 / blkDims.zw).rgba;\n"
	"  float f = step(0.5, frac(uv0.x));\n"
	"  float y = lerp(c0.b, c0.r, f);\n"
	"  float yComp = 1.164f * (y - 0.0625f);\n"
	"  float uComp = c0.g - 0.5f;\n"
	"  float vComp = c0.a - 0.5f;\n"

	"  ocol0 = float4(yComp + (1.596f * vComp),\n"
	"                 yComp - (0.813f * vComp) - (0.391f * uComp),\n"
	"                 yComp + (2.018f * uComp),\n"
	"                 1.0f);\n"
	"}\n",C_COLORMATRIX,C_COLORMATRIX+1);
	s_yuyvToRgbProgram = D3D::CompileAndCreatePixelShader(FProgram, (int)strlen(FProgram));
	if (!s_yuyvToRgbProgram) {
		ERROR_LOG(VIDEO, "Failed to create YUYV to RGB fragment program");
	}
	delete [] FProgram;
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
		if(s_encodingProgramsFailed[format])
		{
			// we already failed to create a shader for this format,
			// so instead of re-trying and showing the same error message every frame, just return.
			return NULL;
		}

		const char* shader = TextureConversionShader::GenerateEncodingShader(format,API_D3D9);

#if defined(_DEBUG) || defined(DEBUGFAST)
		if (g_ActiveConfig.iLog & CONF_SAVESHADERS && shader) {
			static int counter = 0;
			char szTemp[MAX_PATH];
			sprintf(szTemp, "%senc_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), counter++);

			SaveData(szTemp, shader);
		}
#endif
		s_encodingPrograms[format] = D3D::CompileAndCreatePixelShader(shader, (int)strlen(shader));
		if (!s_encodingPrograms[format]) {
			ERROR_LOG(VIDEO, "Failed to create encoding fragment program");
			s_encodingProgramsFailed[format] = true;
		}
	}
	return s_encodingPrograms[format];
}

void Init()
{
	for (unsigned int i = 0; i < NUM_ENCODING_PROGRAMS; i++)
	{
		s_encodingPrograms[i] = NULL;
		s_encodingProgramsFailed[i] = false;
	}
	for (unsigned int i = 0; i < NUM_TRANSFORM_BUFFERS; i++)
	{
		TrnBuffers[i].FBTexture = NULL;
		TrnBuffers[i].RenderSurface = NULL;
		TrnBuffers[i].ReadSurface = NULL;
		TrnBuffers[i].Width = 0;
		TrnBuffers[i].Height = 0;
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

	for (unsigned int i = 0; i < NUM_ENCODING_PROGRAMS; i++)
	{
		if(s_encodingPrograms[i]) 
			s_encodingPrograms[i]->Release();
		s_encodingPrograms[i] = NULL;
	}
	for (unsigned int i = 0; i < NUM_TRANSFORM_BUFFERS; i++)
	{
		if(TrnBuffers[i].RenderSurface != NULL)
			TrnBuffers[i].RenderSurface->Release();
		TrnBuffers[i].RenderSurface = NULL;

		if(TrnBuffers[i].ReadSurface != NULL)
			TrnBuffers[i].ReadSurface->Release();
		TrnBuffers[i].ReadSurface = NULL;

		if(TrnBuffers[i].FBTexture != NULL)
			TrnBuffers[i].FBTexture->Release();
		TrnBuffers[i].FBTexture = NULL;		
		
		TrnBuffers[i].Width = 0;
		TrnBuffers[i].Height = 0;
	}
	WorkingBuffers = 0;
}

void EncodeToRamUsingShader(LPDIRECT3DPIXELSHADER9 shader, LPDIRECT3DTEXTURE9 srcTexture, const TargetRectangle& sourceRc,
				            u8* destAddr, int dstWidth, int dstHeight, int readStride, bool toTexture, bool linearFilter,float Gamma)
{
	HRESULT hr;
	u32 index =0;
	while(index < WorkingBuffers && (TrnBuffers[index].Width != dstWidth || TrnBuffers[index].Height != dstHeight))
		index++;
	
	LPDIRECT3DSURFACE9  s_texConvReadSurface = NULL;
	LPDIRECT3DSURFACE9 Rendersurf = NULL;
	
	if (index >= WorkingBuffers)
	{
		if (WorkingBuffers < NUM_TRANSFORM_BUFFERS)
			WorkingBuffers++;
		if (index >= WorkingBuffers)
			index--;
		if (TrnBuffers[index].RenderSurface != NULL)
		{
			TrnBuffers[index].RenderSurface->Release();
			TrnBuffers[index].RenderSurface = NULL;
		}
		if (TrnBuffers[index].ReadSurface != NULL)
		{
			TrnBuffers[index].ReadSurface->Release();
			TrnBuffers[index].ReadSurface = NULL;
		}
		if (TrnBuffers[index].FBTexture != NULL)
		{
			TrnBuffers[index].FBTexture->Release();
			TrnBuffers[index].FBTexture = NULL;
		}
		TrnBuffers[index].Width = dstWidth;
		TrnBuffers[index].Height = dstHeight;
		D3D::dev->CreateTexture(dstWidth, dstHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
		                                 D3DPOOL_DEFAULT, &TrnBuffers[index].FBTexture, NULL);
		TrnBuffers[index].FBTexture->GetSurfaceLevel(0,&TrnBuffers[index].RenderSurface);
		D3D::dev->CreateOffscreenPlainSurface(dstWidth, dstHeight, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &TrnBuffers[index].ReadSurface, NULL );
	}

	s_texConvReadSurface = TrnBuffers[index].ReadSurface;
	Rendersurf = TrnBuffers[index].RenderSurface;

	hr = D3D::dev->SetDepthStencilSurface(NULL);
	hr = D3D::dev->SetRenderTarget(0, Rendersurf);	

	if (linearFilter)
	{
		D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	}
	else
	{
		D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
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
	D3D::drawShadedTexQuad(srcTexture,&SrcRect,1,1,dstWidth,dstHeight,shader,VertexShaderCache::GetSimpleVertexShader(0), Gamma);
	D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);
	// .. and then read back the results.
	// TODO: make this less slow.

	hr = D3D::dev->GetRenderTargetData(Rendersurf,s_texConvReadSurface);

	D3DLOCKED_RECT drect;
	hr = s_texConvReadSurface->LockRect(&drect, &DstRect, D3DLOCK_READONLY);

	int srcRowsPerBlockRow = readStride / (dstWidth*4); // 4 bytes per pixel

	int readLoops = dstHeight / srcRowsPerBlockRow;
	const u8 *Source = (const u8*)drect.pBits;
	for (int i = 0; i < readLoops; i++)
	{
		for (int j = 0; j < srcRowsPerBlockRow; ++j)
		{
			memcpy(destAddr + j*dstWidth*4, Source, dstWidth*4);
			Source += drect.Pitch;
		}
		destAddr += bpmem.copyMipMapStrideChannels*32;
	}
	
	hr = s_texConvReadSurface->UnlockRect();
}

int EncodeToRamFromTexture(u32 address,LPDIRECT3DTEXTURE9 source_texture, u32 SourceW, u32 SourceH, bool bFromZBuffer, bool bIsIntensityFmt, u32 copyfmt, int bScaleByHalf, const EFBRectangle& source)
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

	LPDIRECT3DPIXELSHADER9 texconv_shader = GetOrCreateEncodingShader(format);
	if (!texconv_shader)
		return 0;

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
	TextureConversionShader::SetShaderParameters(
		(float)expandedWidth,
		(float)Renderer::EFBToScaledY(expandedHeight), // TODO: Why do we scale this?
		(float)Renderer::EFBToScaledX(source.left),
		(float)Renderer::EFBToScaledY(source.top),
		Renderer::EFBToScaledXf(sampleStride),
		Renderer::EFBToScaledYf(sampleStride),
		(float)SourceW,
		(float)SourceH);

	TargetRectangle scaledSource;
	scaledSource.top = 0;
	scaledSource.bottom = expandedHeight;
	scaledSource.left = 0;
	scaledSource.right = expandedWidth / samples;
	int cacheBytes = 32;
	if ((format & 0x0f) == 6)
		cacheBytes = 64;

	int readStride = (expandedWidth * cacheBytes) / TexDecoder_GetBlockWidthInTexels(format);
	EncodeToRamUsingShader(texconv_shader, source_texture, scaledSource, dest_ptr, expandedWidth / samples, expandedHeight, readStride, true, bScaleByHalf > 0,1.0f);
	return size_in_bytes; // TODO: D3D11 is calculating this value differently!
}

void EncodeToRamYUYV(LPDIRECT3DTEXTURE9 srcTexture, const TargetRectangle& sourceRc, u8* destAddr, int dstWidth, int dstHeight,float Gamma)
{
	TextureConversionShader::SetShaderParameters(
		(float)dstWidth, 
		(float)dstHeight, 
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		(float)Renderer::GetTargetWidth(),
		(float)Renderer::GetTargetHeight());
	g_renderer->ResetAPIState();
	EncodeToRamUsingShader(s_rgbToYuyvProgram, srcTexture, sourceRc, destAddr,
		dstWidth / 2, dstHeight, dstWidth * 2, false, false,Gamma);
	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
	g_renderer->RestoreAPIState();
}


// Should be scale free.
void DecodeToTexture(u32 xfbAddr, int srcWidth, int srcHeight, LPDIRECT3DTEXTURE9 destTexture)
{
	u8* srcAddr = Memory::GetPointer(xfbAddr);
	if (!srcAddr)
	{
		WARN_LOG(VIDEO, "Tried to decode from invalid memory address");
		return;
	}

	int srcFmtWidth = srcWidth / 2;

	g_renderer->ResetAPIState(); // reset any game specific settings
	LPDIRECT3DTEXTURE9 s_srcTexture = D3D::CreateTexture2D(srcAddr, srcFmtWidth, srcHeight, srcFmtWidth, D3DFMT_A8R8G8B8, false);
	LPDIRECT3DSURFACE9 Rendersurf = NULL;
	destTexture->GetSurfaceLevel(0,&Rendersurf);
	D3D::dev->SetDepthStencilSurface(NULL);
	D3D::dev->SetRenderTarget(0, Rendersurf);

	D3DVIEWPORT9 vp;

	// Stretch picture with increased internal resolution
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = srcWidth;
	vp.Height = srcHeight;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	D3D::dev->SetViewport(&vp);
	RECT destrect;
	destrect.bottom = srcHeight;
	destrect.left = 0;
	destrect.right = srcWidth;
	destrect.top = 0;
	
	RECT sourcerect;
	sourcerect.bottom = srcHeight;
	sourcerect.left = 0;
	sourcerect.right = srcFmtWidth;
	sourcerect.top = 0;

	D3D::ChangeSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	D3D::ChangeSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		
	TextureConversionShader::SetShaderParameters(
		(float)srcFmtWidth,
		(float)srcHeight,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		(float)srcFmtWidth,
		(float)srcHeight);
	D3D::drawShadedTexQuad(
		s_srcTexture,
		&sourcerect,
		1,
		1,
		srcWidth,
		srcHeight,
		s_yuyvToRgbProgram,
		VertexShaderCache::GetSimpleVertexShader(0));


	D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);
	D3D::RefreshSamplerState(0, D3DSAMP_MAGFILTER);
	D3D::SetTexture(0,NULL);
	D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());	
	g_renderer->RestoreAPIState();
	Rendersurf->Release();
	s_srcTexture->Release();
}

}  // namespace

}  // namespace DX9
