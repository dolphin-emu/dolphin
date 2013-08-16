// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include <stdio.h>
#include <math.h>
#include <locale.h>
#ifdef __APPLE__
	#include <xlocale.h>
#endif

#include "TextureConversionShader.h"
#include "TextureDecoder.h"
#include "PixelShaderManager.h"
#include "PixelShaderGen.h"
#include "BPMemory.h"
#include "RenderBase.h"
#include "VideoConfig.h"

#define WRITE p+=sprintf

static char text[16384];
static bool IntensityConstantAdded =  false;
static int s_incrementSampleXCount = 0;

namespace TextureConversionShader
{

u16 GetEncodedSampleCount(u32 format)
{
	switch (format)
	{
	case GX_TF_I4: return 8;
	case GX_TF_I8: return 4;
	case GX_TF_IA4: return 4;
	case GX_TF_IA8: return 2;
	case GX_TF_RGB565: return 2;
	case GX_TF_RGB5A3: return 2;
	case GX_TF_RGBA8: return 1;
	case GX_CTF_R4: return 8;
	case GX_CTF_RA4: return 4;
	case GX_CTF_RA8: return 2;
	case GX_CTF_A8: return 4;
	case GX_CTF_R8: return 4;
	case GX_CTF_G8: return 4;
	case GX_CTF_B8: return 4;
	case GX_CTF_RG8: return 2;
	case GX_CTF_GB8: return 2;
	case GX_TF_Z8: return 4;
	case GX_TF_Z16: return 2;
	case GX_TF_Z24X8: return 1;
	case GX_CTF_Z4: return 8;
	case GX_CTF_Z8M: return 4;
	case GX_CTF_Z8L: return 4;
	case GX_CTF_Z16L: return 2;
	default: return 1;
	}
}

const char* WriteRegister(API_TYPE ApiType, const char *prefix, const u32 num)
{
	if (ApiType == API_OPENGL)
		return ""; // Once we switch to GLSL 1.3 we can do something here
	static char result[64];
	sprintf(result, " : register(%s%d)", prefix, num);
	return result;
}

// block dimensions : widthStride, heightStride 
// texture dims : width, height, x offset, y offset
void WriteSwizzler(char*& p, u32 format, API_TYPE ApiType)
{
	// [0] left, top, right, bottom of source rectangle within source texture
	// [1] width and height of destination texture in pixels
	// Two were merged for GLSL
	WRITE(p, "uniform float4 " I_COLORS"[2] %s;\n", WriteRegister(ApiType, "c", C_COLORS));

	float blkW = (float)TexDecoder_GetBlockWidthInTexels(format);
	float blkH = (float)TexDecoder_GetBlockHeightInTexels(format);
	float samples = (float)GetEncodedSampleCount(format);
	if (ApiType == API_OPENGL)
	{
		WRITE(p, "#define samp0 samp9\n");
		WRITE(p, "uniform sampler2DRect samp0;\n");
	}
	else if (ApiType & API_D3D9)
	{
		WRITE(p,"uniform sampler samp0 : register(s0);\n");
	}
	else
	{
		WRITE(p,"sampler samp0 : register(s0);\n");
		WRITE(p, "Texture2D Tex0 : register(t0);\n");
	}

	if (ApiType == API_OPENGL)
	{
		WRITE(p, "  COLOROUT(ocol0)\n");
		WRITE(p, "  VARYIN float2 uv0;\n");
		WRITE(p, "void main()\n");
	}
	else
	{
		WRITE(p,"void main(\n");
		if (ApiType != API_D3D11)
		{
			WRITE(p,"  out float4 ocol0 : COLOR0,\n");
		}
		else
		{
			WRITE(p,"  out float4 ocol0 : SV_Target,\n");
		}
		WRITE(p,"  in float2 uv0 : TEXCOORD0)\n");
	}

	WRITE(p, "{\n"    
	"  float2 sampleUv;\n"
	"  float2 uv1 = floor(uv0);\n");

	WRITE(p, "  uv1.x = uv1.x * %f;\n", samples);

	WRITE(p, "  float xl =  floor(uv1.x / %f);\n", blkW);
	WRITE(p, "  float xib = uv1.x - (xl * %f);\n", blkW);
	WRITE(p, "  float yl = floor(uv1.y / %f);\n", blkH);
	WRITE(p, "  float yb = yl * %f;\n", blkH);
	WRITE(p, "  float yoff = uv1.y - yb;\n");
	WRITE(p, "  float xp = uv1.x + (yoff * " I_COLORS"[1].x);\n");
	WRITE(p, "  float xel = floor(xp / %f);\n", blkW);
	WRITE(p, "  float xb = floor(xel / %f);\n", blkH);
	WRITE(p, "  float xoff = xel - (xb * %f);\n", blkH);

	WRITE(p, "  sampleUv.x = xib + (xb * %f);\n", blkW);
	WRITE(p, "  sampleUv.y = yb + xoff;\n");

	WRITE(p, "  sampleUv = sampleUv * " I_COLORS"[0].xy;\n");

	if (ApiType == API_OPENGL)
		WRITE(p,"  sampleUv.y = " I_COLORS"[1].y - sampleUv.y;\n");

	WRITE(p, "  sampleUv = sampleUv + " I_COLORS"[1].zw;\n");

	if (ApiType != API_OPENGL)
	{
		WRITE(p, "  sampleUv = sampleUv + float2(0.0f,1.0f);\n");// still to determine the reason for this
		WRITE(p, "  sampleUv = sampleUv / " I_COLORS"[0].zw;\n");
	}
}

// block dimensions : widthStride, heightStride 
// texture dims : width, height, x offset, y offset
void Write32BitSwizzler(char*& p, u32 format, API_TYPE ApiType)
{	
	// [0] left, top, right, bottom of source rectangle within source texture
	// [1] width and height of destination texture in pixels
	// Two were merged for GLSL
	WRITE(p, "uniform float4 " I_COLORS"[2] %s;\n", WriteRegister(ApiType, "c", C_COLORS));

	float blkW = (float)TexDecoder_GetBlockWidthInTexels(format);
	float blkH = (float)TexDecoder_GetBlockHeightInTexels(format);

	// 32 bit textures (RGBA8 and Z24) are store in 2 cache line increments
	if (ApiType == API_OPENGL)
	{
		WRITE(p, "#define samp0 samp9\n");
		WRITE(p, "uniform sampler2DRect samp0;\n");
	}
	else if (ApiType & API_D3D9)
	{
		WRITE(p,"uniform sampler samp0 : register(s0);\n");
	}
	else
	{
		WRITE(p,"sampler samp0 : register(s0);\n");
		WRITE(p, "Texture2D Tex0 : register(t0);\n");
	}

	if (ApiType == API_OPENGL)
	{
		WRITE(p, "  out float4 ocol0;\n");
		WRITE(p, "  VARYIN float2 uv0;\n");
		WRITE(p, "void main()\n");
	}
	else
	{
		WRITE(p,"void main(\n");
		if(ApiType != API_D3D11)
		{
			WRITE(p,"  out float4 ocol0 : COLOR0,\n");
		}
		else
		{
			WRITE(p,"  out float4 ocol0 : SV_Target,\n");
		}
		WRITE(p,"  in float2 uv0 : TEXCOORD0)\n");
	}


	WRITE(p, "{\n"    
	"  float2 sampleUv;\n"
	"  float2 uv1 = floor(uv0);\n");

	WRITE(p, "  float yl = floor(uv1.y / %f);\n", blkH);
	WRITE(p, "  float yb = yl * %f;\n", blkH);
	WRITE(p, "  float yoff = uv1.y - yb;\n");
	WRITE(p, "  float xp = uv1.x + (yoff * " I_COLORS"[1].x);\n");
	WRITE(p, "  float xel = floor(xp / 2.0f);\n");
	WRITE(p, "  float xb = floor(xel / %f);\n", blkH);
	WRITE(p, "  float xoff = xel - (xb * %f);\n", blkH);

	WRITE(p, "  float x2 = uv1.x * 2.0f;\n");
	WRITE(p, "  float xl = floor(x2 / %f);\n", blkW);	
	WRITE(p, "  float xib = x2 - (xl * %f);\n", blkW);
	WRITE(p, "  float halfxb = floor(xb / 2.0f);\n");

	WRITE(p, "  sampleUv.x = xib + (halfxb * %f);\n", blkW);
	WRITE(p, "  sampleUv.y = yb + xoff;\n");
	WRITE(p, "  sampleUv = sampleUv * " I_COLORS"[0].xy;\n");

	if (ApiType == API_OPENGL)
		WRITE(p,"  sampleUv.y = " I_COLORS"[1].y - sampleUv.y;\n");

	WRITE(p, "  sampleUv = sampleUv + " I_COLORS"[1].zw;\n");

	if (ApiType != API_OPENGL)
	{
		WRITE(p, "  sampleUv = sampleUv + float2(0.0f,1.0f);\n");// still to determine the reason for this
		WRITE(p, "  sampleUv = sampleUv / " I_COLORS"[0].zw;\n");
	}
}

void WriteSampleColor(char*& p, const char* colorComp, const char* dest, API_TYPE ApiType)
{
	const char* texSampleOpName;
	if (ApiType & API_D3D9)
		texSampleOpName = "tex2D";
	else if (ApiType == API_D3D11)
		texSampleOpName = "tex0.Sample";
	else
		texSampleOpName = "texture2DRect";

	// the increment of sampleUv.x is delayed, so we perform it here. see WriteIncrementSampleX.
	const char* texSampleIncrementUnit;
	if (ApiType != API_OPENGL)
		texSampleIncrementUnit = I_COLORS"[0].x / " I_COLORS"[0].z";
	else
		texSampleIncrementUnit = I_COLORS"[0].x";

	WRITE(p, "  %s = %s(samp0, sampleUv + float2(%d.0f * (%s), 0.0f)).%s;\n",
		dest, texSampleOpName, s_incrementSampleXCount, texSampleIncrementUnit, colorComp);
}

void WriteColorToIntensity(char*& p, const char* src, const char* dest)
{
	if (!IntensityConstantAdded)
	{
		WRITE(p, "  float4 IntensityConst = float4(0.257f,0.504f,0.098f,0.0625f);\n");
		IntensityConstantAdded = true;
	}
	WRITE(p, "  %s = dot(IntensityConst.rgb, %s.rgb);\n", dest, src);
	// don't add IntensityConst.a yet, because doing it later is faster and uses less instructions, due to vectorization
}

void WriteIncrementSampleX(char*& p,API_TYPE ApiType)
{
	// the shader compiler apparently isn't smart or aggressive enough to recognize that:
	//    foo1 = lookup(x)
	//    x = x + increment;
	//    foo2 = lookup(x)
	//    x = x + increment;
	//    foo3 = lookup(x)
	// can be replaced with this:
	//    foo1 = lookup(x + 0.0 * increment)
	//    foo2 = lookup(x + 1.0 * increment)
	//    foo3 = lookup(x + 2.0 * increment)
	// which looks like the same operations but uses considerably fewer ALU instruction slots.
	// thus, instead of using the former method, we only increment a counter internally here,
	// and we wait until WriteSampleColor to write out the constant multiplier
	// to achieve the increment as in the latter case.
	s_incrementSampleXCount++;
}

void WriteToBitDepth(char*& p, u8 depth, const char* src, const char* dest)
{
	float result = 255 / pow(2.0f, (8 - depth));
	WRITE(p, "  %s = floor(%s * %ff);\n", dest, src, result);
}

void WriteEncoderEnd(char* p, API_TYPE ApiType)
{
	WRITE(p, "}\n");
	IntensityConstantAdded = false;
	s_incrementSampleXCount = 0;
}

void WriteI8Encoder(char* p, API_TYPE ApiType)
{
	WriteSwizzler(p, GX_TF_I8, ApiType);
	WRITE(p, "  float3 texSample;\n");	

	WriteSampleColor(p, "rgb", "texSample", ApiType);
	WriteColorToIntensity(p, "texSample", "ocol0.b");
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "rgb", "texSample", ApiType);
	WriteColorToIntensity(p, "texSample", "ocol0.g");
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "rgb", "texSample", ApiType);
	WriteColorToIntensity(p, "texSample", "ocol0.r");
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "rgb", "texSample", ApiType);
	WriteColorToIntensity(p, "texSample", "ocol0.a");

	WRITE(p, "  ocol0.rgba += IntensityConst.aaaa;\n"); // see WriteColorToIntensity

	WriteEncoderEnd(p, ApiType);
}

void WriteI4Encoder(char* p, API_TYPE ApiType)
{
	WriteSwizzler(p, GX_TF_I4, ApiType);
	WRITE(p, "  float3 texSample;\n");
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, "rgb", "texSample", ApiType);
	WriteColorToIntensity(p, "texSample", "color0.b");
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "rgb", "texSample", ApiType);
	WriteColorToIntensity(p, "texSample", "color1.b");
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "rgb", "texSample", ApiType);
	WriteColorToIntensity(p, "texSample", "color0.g");
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "rgb", "texSample", ApiType);
	WriteColorToIntensity(p, "texSample", "color1.g");
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "rgb", "texSample", ApiType);
	WriteColorToIntensity(p, "texSample", "color0.r");
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "rgb", "texSample", ApiType);
	WriteColorToIntensity(p, "texSample", "color1.r");
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "rgb", "texSample", ApiType);
	WriteColorToIntensity(p, "texSample", "color0.a");
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "rgb", "texSample", ApiType);
	WriteColorToIntensity(p, "texSample", "color1.a");

	WRITE(p, "  color0.rgba += IntensityConst.aaaa;\n");
	WRITE(p, "  color1.rgba += IntensityConst.aaaa;\n");

	WriteToBitDepth(p, 4, "color0", "color0");
	WriteToBitDepth(p, 4, "color1", "color1");

	WRITE(p, "  ocol0 = (color0 * 16.0f + color1) / 255.0f;\n");
	WriteEncoderEnd(p, ApiType);
}

void WriteIA8Encoder(char* p,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_TF_IA8, ApiType);
	WRITE(p, "  float4 texSample;\n");	

	WriteSampleColor(p, "rgba", "texSample", ApiType);
	WRITE(p, "  ocol0.b = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "ocol0.g");
	WriteIncrementSampleX(p, ApiType);	

	WriteSampleColor(p, "rgba", "texSample", ApiType);
	WRITE(p, "  ocol0.r = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "ocol0.a");

	WRITE(p, "  ocol0.ga += IntensityConst.aa;\n");

	WriteEncoderEnd(p, ApiType);
}

void WriteIA4Encoder(char* p,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_TF_IA4, ApiType);
	WRITE(p, "  float4 texSample;\n");
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, "rgba", "texSample", ApiType);
	WRITE(p, "  color0.b = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "color1.b");
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "rgba", "texSample", ApiType);
	WRITE(p, "  color0.g = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "color1.g");
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "rgba", "texSample", ApiType);
	WRITE(p, "  color0.r = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "color1.r");
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "rgba", "texSample", ApiType);
	WRITE(p, "  color0.a = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "color1.a");

	WRITE(p, "  color1.rgba += IntensityConst.aaaa;\n");

	WriteToBitDepth(p, 4, "color0", "color0");
	WriteToBitDepth(p, 4, "color1", "color1");

	WRITE(p, "  ocol0 = (color0 * 16.0f + color1) / 255.0f;\n");
	WriteEncoderEnd(p, ApiType);
}

void WriteRGB565Encoder(char* p,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_TF_RGB565, ApiType);

	WriteSampleColor(p, "rgb", "float3 texSample0", ApiType);
	WriteIncrementSampleX(p, ApiType);
	WriteSampleColor(p, "rgb", "float3 texSample1", ApiType);
	WRITE(p, "  float2 texRs = float2(texSample0.r, texSample1.r);\n");
	WRITE(p, "  float2 texGs = float2(texSample0.g, texSample1.g);\n");
	WRITE(p, "  float2 texBs = float2(texSample0.b, texSample1.b);\n");
  
	WriteToBitDepth(p, 6, "texGs", "float2 gInt");
	WRITE(p, "  float2 gUpper = floor(gInt / 8.0f);\n");
	WRITE(p, "  float2 gLower = gInt - gUpper * 8.0f;\n");

	WriteToBitDepth(p, 5, "texRs", "ocol0.br");
	WRITE(p, "  ocol0.br = ocol0.br * 8.0f + gUpper;\n");
	WriteToBitDepth(p, 5, "texBs", "ocol0.ga");
	WRITE(p, "  ocol0.ga = ocol0.ga + gLower * 32.0f;\n");

	WRITE(p, "  ocol0 = ocol0 / 255.0f;\n");
	WriteEncoderEnd(p, ApiType);
}

void WriteRGB5A3Encoder(char* p,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_TF_RGB5A3, ApiType);

	WRITE(p, "  float4 texSample;\n");
	WRITE(p, "  float color0;\n");
	WRITE(p, "  float gUpper;\n");
	WRITE(p, "  float gLower;\n");

	WriteSampleColor(p, "rgba", "texSample", ApiType);

	// 0.8784 = 224 / 255 which is the maximum alpha value that can be represented in 3 bits
	WRITE(p, "if(texSample.a > 0.878f) {\n");

	WriteToBitDepth(p, 5, "texSample.g", "color0");
	WRITE(p, "  gUpper = floor(color0 / 8.0f);\n");
	WRITE(p, "  gLower = color0 - gUpper * 8.0f;\n");

	WriteToBitDepth(p, 5, "texSample.r", "ocol0.b");
	WRITE(p, "  ocol0.b = ocol0.b * 4.0f + gUpper + 128.0f;\n");
	WriteToBitDepth(p, 5, "texSample.b", "ocol0.g");
	WRITE(p, "  ocol0.g = ocol0.g + gLower * 32.0f;\n");

	WRITE(p, "} else {\n");

	WriteToBitDepth(p, 4, "texSample.r", "ocol0.b");
	WriteToBitDepth(p, 4, "texSample.b", "ocol0.g");

	WriteToBitDepth(p, 3, "texSample.a", "color0");
	WRITE(p, "ocol0.b = ocol0.b + color0 * 16.0f;\n");
	WriteToBitDepth(p, 4, "texSample.g", "color0");
	WRITE(p, "ocol0.g = ocol0.g + color0 * 16.0f;\n");

	WRITE(p, "}\n");


	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "rgba", "texSample", ApiType);

	WRITE(p, "if(texSample.a > 0.878f) {\n");

	WriteToBitDepth(p, 5, "texSample.g", "color0");
	WRITE(p, "  gUpper = floor(color0 / 8.0f);\n");
	WRITE(p, "  gLower = color0 - gUpper * 8.0f;\n");

	WriteToBitDepth(p, 5, "texSample.r", "ocol0.r");
	WRITE(p, "  ocol0.r = ocol0.r * 4.0f + gUpper + 128.0f;\n");
	WriteToBitDepth(p, 5, "texSample.b", "ocol0.a");
	WRITE(p, "  ocol0.a = ocol0.a + gLower * 32.0f;\n");

	WRITE(p, "} else {\n");

	WriteToBitDepth(p, 4, "texSample.r", "ocol0.r");
	WriteToBitDepth(p, 4, "texSample.b", "ocol0.a");

	WriteToBitDepth(p, 3, "texSample.a", "color0");
	WRITE(p, "ocol0.r = ocol0.r + color0 * 16.0f;\n");
	WriteToBitDepth(p, 4, "texSample.g", "color0");
	WRITE(p, "ocol0.a = ocol0.a + color0 * 16.0f;\n");

	WRITE(p, "}\n");

	WRITE(p, "  ocol0 = ocol0 / 255.0f;\n");
	WriteEncoderEnd(p, ApiType);
}

void WriteRGBA4443Encoder(char* p,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_TF_RGB5A3, ApiType);

	WRITE(p, "  float4 texSample;\n");
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, "rgba", "texSample", ApiType);
	WriteToBitDepth(p, 3, "texSample.a", "color0.b");
	WriteToBitDepth(p, 4, "texSample.r", "color1.b");
	WriteToBitDepth(p, 4, "texSample.g", "color0.g");
	WriteToBitDepth(p, 4, "texSample.b", "color1.g");

	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "rgba", "texSample", ApiType);
	WriteToBitDepth(p, 3, "texSample.a", "color0.r");
	WriteToBitDepth(p, 4, "texSample.r", "color1.r");
	WriteToBitDepth(p, 4, "texSample.g", "color0.a");
	WriteToBitDepth(p, 4, "texSample.b", "color1.a");

	WRITE(p, "  ocol0 = (color0 * 16.0f + color1) / 255.0f;\n");
	WriteEncoderEnd(p, ApiType);
}

void WriteRGBA8Encoder(char* p,API_TYPE ApiType)
{
	Write32BitSwizzler(p, GX_TF_RGBA8, ApiType);

	WRITE(p, "  float cl1 = xb - (halfxb * 2.0f);\n");
	WRITE(p, "  float cl0 = 1.0f - cl1;\n");

	WRITE(p, "  float4 texSample;\n");
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, "rgba", "texSample", ApiType);
	WRITE(p, "  color0.b = texSample.a;\n");
	WRITE(p, "  color0.g = texSample.r;\n");
	WRITE(p, "  color1.b = texSample.g;\n");
	WRITE(p, "  color1.g = texSample.b;\n");

	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "rgba", "texSample", ApiType);
	WRITE(p, "  color0.r = texSample.a;\n");
	WRITE(p, "  color0.a = texSample.r;\n");
	WRITE(p, "  color1.r = texSample.g;\n");
	WRITE(p, "  color1.a = texSample.b;\n");

	WRITE(p, "  ocol0 = (cl0 * color0) + (cl1 * color1);\n");

	WriteEncoderEnd(p, ApiType);
}

void WriteC4Encoder(char* p, const char* comp,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_CTF_R4, ApiType);
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, comp, "color0.b", ApiType);
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, comp, "color1.b", ApiType);
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, comp, "color0.g", ApiType);
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, comp, "color1.g", ApiType);
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, comp, "color0.r", ApiType);
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, comp, "color1.r", ApiType);
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, comp, "color0.a", ApiType);
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, comp, "color1.a", ApiType);

	WriteToBitDepth(p, 4, "color0", "color0");
	WriteToBitDepth(p, 4, "color1", "color1");

	WRITE(p, "  ocol0 = (color0 * 16.0f + color1) / 255.0f;\n");
	WriteEncoderEnd(p, ApiType);
}

void WriteC8Encoder(char* p, const char* comp,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_CTF_R8, ApiType);

	WriteSampleColor(p, comp, "ocol0.b", ApiType);
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, comp, "ocol0.g", ApiType);
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, comp, "ocol0.r", ApiType);
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, comp, "ocol0.a", ApiType);

	WriteEncoderEnd(p, ApiType);
}

void WriteCC4Encoder(char* p, const char* comp,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_CTF_RA4, ApiType);
	WRITE(p, "  float2 texSample;\n");
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, comp, "texSample", ApiType);
	WRITE(p, "  color0.b = texSample.x;\n");
	WRITE(p, "  color1.b = texSample.y;\n");
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, comp, "texSample", ApiType);
	WRITE(p, "  color0.g = texSample.x;\n");
	WRITE(p, "  color1.g = texSample.y;\n");
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, comp, "texSample", ApiType);
	WRITE(p, "  color0.r = texSample.x;\n");
	WRITE(p, "  color1.r = texSample.y;\n");
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, comp, "texSample", ApiType);
	WRITE(p, "  color0.a = texSample.x;\n");
	WRITE(p, "  color1.a = texSample.y;\n");

	WriteToBitDepth(p, 4, "color0", "color0");
	WriteToBitDepth(p, 4, "color1", "color1");

	WRITE(p, "  ocol0 = (color0 * 16.0f + color1) / 255.0f;\n");
	WriteEncoderEnd(p, ApiType);
}

void WriteCC8Encoder(char* p, const char* comp, API_TYPE ApiType)
{
	WriteSwizzler(p, GX_CTF_RA8, ApiType);

	WriteSampleColor(p, comp, "ocol0.bg", ApiType);
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, comp, "ocol0.ra", ApiType);

	WriteEncoderEnd(p, ApiType);
}

void WriteZ8Encoder(char* p, const char* multiplier,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_CTF_Z8M, ApiType);

	WRITE(p, " float depth;\n");

	WriteSampleColor(p, "b", "depth", ApiType);
	WRITE(p, "ocol0.b = frac(depth * %s);\n", multiplier);
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "b", "depth", ApiType);
	WRITE(p, "ocol0.g = frac(depth * %s);\n", multiplier);
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "b", "depth", ApiType);
	WRITE(p, "ocol0.r = frac(depth * %s);\n", multiplier);
	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "b", "depth", ApiType);
	WRITE(p, "ocol0.a = frac(depth * %s);\n", multiplier);

	WriteEncoderEnd(p, ApiType);
}

void WriteZ16Encoder(char* p,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_TF_Z16, ApiType);

	WRITE(p, "  float depth;\n");
	WRITE(p, "  float3 expanded;\n");

	// byte order is reversed

	WriteSampleColor(p, "b", "depth", ApiType);

	WRITE(p, "  depth *= 16777215.0f;\n");
	WRITE(p, "  expanded.r = floor(depth / (256.0f * 256.0f));\n");
	WRITE(p, "  depth -= expanded.r * 256.0f * 256.0f;\n");
	WRITE(p, "  expanded.g = floor(depth / 256.0f);\n");

	WRITE(p, "  ocol0.b = expanded.g / 255.0f;\n");
	WRITE(p, "  ocol0.g = expanded.r / 255.0f;\n");

	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "b", "depth", ApiType);

	WRITE(p, "  depth *= 16777215.0f;\n");
	WRITE(p, "  expanded.r = floor(depth / (256.0f * 256.0f));\n");
	WRITE(p, "  depth -= expanded.r * 256.0f * 256.0f;\n");
	WRITE(p, "  expanded.g = floor(depth / 256.0f);\n");

	WRITE(p, "  ocol0.r = expanded.g / 255.0f;\n");
	WRITE(p, "  ocol0.a = expanded.r / 255.0f;\n");

	WriteEncoderEnd(p, ApiType);
}

void WriteZ16LEncoder(char* p,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_CTF_Z16L, ApiType);

	WRITE(p, "  float depth;\n");
	WRITE(p, "  float3 expanded;\n");

	// byte order is reversed

	WriteSampleColor(p, "b", "depth", ApiType);

	WRITE(p, "  depth *= 16777215.0f;\n");
	WRITE(p, "  expanded.r = floor(depth / (256.0f * 256.0f));\n");
	WRITE(p, "  depth -= expanded.r * 256.0f * 256.0f;\n");
	WRITE(p, "  expanded.g = floor(depth / 256.0f);\n");
	WRITE(p, "  depth -= expanded.g * 256.0f;\n");
	WRITE(p, "  expanded.b = depth;\n");

	WRITE(p, "  ocol0.b = expanded.b / 255.0f;\n");
	WRITE(p, "  ocol0.g = expanded.g / 255.0f;\n");

	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "b", "depth", ApiType);

	WRITE(p, "  depth *= 16777215.0f;\n");
	WRITE(p, "  expanded.r = floor(depth / (256.0f * 256.0f));\n");
	WRITE(p, "  depth -= expanded.r * 256.0f * 256.0f;\n");
	WRITE(p, "  expanded.g = floor(depth / 256.0f);\n");
	WRITE(p, "  depth -= expanded.g * 256.0f;\n");
	WRITE(p, "  expanded.b = depth;\n");

	WRITE(p, "  ocol0.r = expanded.b;\n");
	WRITE(p, "  ocol0.a = expanded.g;\n");

	WriteEncoderEnd(p, ApiType);
}

void WriteZ24Encoder(char* p, API_TYPE ApiType)
{
	Write32BitSwizzler(p, GX_TF_Z24X8, ApiType);

	WRITE(p, "  float cl = xb - (halfxb * 2.0f);\n");

	WRITE(p, "  float depth0;\n");
	WRITE(p, "  float depth1;\n");
	WRITE(p, "  float3 expanded0;\n");
	WRITE(p, "  float3 expanded1;\n");

	WriteSampleColor(p, "b", "depth0", ApiType);
	WriteIncrementSampleX(p, ApiType);
	WriteSampleColor(p, "b", "depth1", ApiType);

	for (int i = 0; i < 2; i++)
	{
		WRITE(p, "  depth%i *= 16777215.0f;\n", i);

		WRITE(p, "  expanded%i.r = floor(depth%i / (256.0f * 256.0f));\n", i, i);
		WRITE(p, "  depth%i -= expanded%i.r * 256.0f * 256.0f;\n", i, i);
		WRITE(p, "  expanded%i.g = floor(depth%i / 256.0f);\n", i, i);
		WRITE(p, "  depth%i -= expanded%i.g * 256.0f;\n", i, i);
		WRITE(p, "  expanded%i.b = depth%i;\n", i, i);
	}

	WRITE(p, "  if(cl > 0.5f) {\n");
	// upper 16
	WRITE(p, "     ocol0.b = expanded0.g / 255.0f;\n");
	WRITE(p, "     ocol0.g = expanded0.b / 255.0f;\n");
	WRITE(p, "     ocol0.r = expanded1.g / 255.0f;\n");
	WRITE(p, "     ocol0.a = expanded1.b / 255.0f;\n");
	WRITE(p, "  } else {\n");
	// lower 8
	WRITE(p, "     ocol0.b = 1.0f;\n");
	WRITE(p, "     ocol0.g = expanded0.r / 255.0f;\n");
	WRITE(p, "     ocol0.r = 1.0f;\n");
	WRITE(p, "     ocol0.a = expanded1.r / 255.0f;\n");
	WRITE(p, "  }\n");

	WriteEncoderEnd(p, ApiType);
}

const char *GenerateEncodingShader(u32 format,API_TYPE ApiType)
{
#ifndef ANDROID
	locale_t locale = newlocale(LC_NUMERIC_MASK, "C", NULL); // New locale for compilation
	locale_t old_locale = uselocale(locale); // Apply the locale for this thread
#endif
	text[sizeof(text) - 1] = 0x7C;  // canary

	char *p = text;

	switch (format)
	{
	case GX_TF_I4:
		WriteI4Encoder(p, ApiType);
		break;
	case GX_TF_I8:
		WriteI8Encoder(p, ApiType);
		break;
	case GX_TF_IA4:
		WriteIA4Encoder(p, ApiType);
		break;
	case GX_TF_IA8:
		WriteIA8Encoder(p, ApiType);
		break;
	case GX_TF_RGB565:
		WriteRGB565Encoder(p, ApiType);
		break;
	case GX_TF_RGB5A3:
		WriteRGB5A3Encoder(p, ApiType);
		break;
	case GX_TF_RGBA8:
		WriteRGBA8Encoder(p, ApiType);
		break;
	case GX_CTF_R4:
		WriteC4Encoder(p, "r", ApiType);
		break;
	case GX_CTF_RA4:
		WriteCC4Encoder(p, "ar", ApiType);
		break;
	case GX_CTF_RA8:
		WriteCC8Encoder(p, "ar", ApiType);
		break;
	case GX_CTF_A8:
		WriteC8Encoder(p, "a", ApiType);
		break;
	case GX_CTF_R8:
		WriteC8Encoder(p, "r", ApiType);
		break;
	case GX_CTF_G8:
		WriteC8Encoder(p, "g", ApiType);
		break;
	case GX_CTF_B8:
		WriteC8Encoder(p, "b", ApiType);
		break;
	case GX_CTF_RG8:
		WriteCC8Encoder(p, "rg", ApiType);
		break;
	case GX_CTF_GB8:
		WriteCC8Encoder(p, "gb", ApiType);
		break;
	case GX_TF_Z8:
		WriteC8Encoder(p, "b", ApiType);
		break;
	case GX_TF_Z16:
		WriteZ16Encoder(p, ApiType);
		break;
	case GX_TF_Z24X8:
		WriteZ24Encoder(p, ApiType);
		break;
	case GX_CTF_Z4:
		WriteC4Encoder(p, "b", ApiType);
		break;
	case GX_CTF_Z8M:
		WriteZ8Encoder(p, "256.0f", ApiType);
		break;
	case GX_CTF_Z8L:
		WriteZ8Encoder(p, "65536.0f" , ApiType);
		break;
	case GX_CTF_Z16L:
		WriteZ16LEncoder(p, ApiType);
		break;
	default:
		PanicAlert("Unknown texture copy format: 0x%x\n", format);
		break;		
	}

	if (text[sizeof(text) - 1] != 0x7C)
		PanicAlert("TextureConversionShader generator - buffer too small, canary has been eaten!");
	
#ifndef ANDROID
	uselocale(old_locale); // restore locale
	freelocale(locale);
#endif
	return text;
}

void SetShaderParameters(float width, float height, float offsetX, float offsetY, float widthStride, float heightStride,float buffW,float buffH)
{
	g_renderer->SetPSConstant4f(C_COLORMATRIX, widthStride, heightStride, buffW, buffH);
	g_renderer->SetPSConstant4f(C_COLORMATRIX + 1, width, (height - 1), offsetX, offsetY);
}

}  // namespace
