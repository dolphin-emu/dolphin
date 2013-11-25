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

// block dimensions : widthStride, heightStride
// texture dims : width, height, x offset, y offset
void WriteSwizzler(char*& p, u32 format, API_TYPE ApiType)
{
	// left, top, of source rectangle within source texture
	// width of the destination rectangle, scale_factor (1 or 2)
	WRITE(p, "uniform float4 " I_COLORS";\n");

	int blkW = TexDecoder_GetBlockWidthInTexels(format);
	int blkH = TexDecoder_GetBlockHeightInTexels(format);
	int samples = GetEncodedSampleCount(format);
	// 32 bit textures (RGBA8 and Z24) are store in 2 cache line increments
	int factor = samples == 1 ? 2 : 1;
	if (ApiType == API_OPENGL)
	{
		WRITE(p, "#define samp0 samp9\n");
		WRITE(p, "uniform sampler2D samp0;\n");

		WRITE(p, "  out vec4 ocol0;\n");
		WRITE(p, "void main()\n");
	}
	else // D3D
	{
		WRITE(p,"sampler samp0 : register(s0);\n");
		WRITE(p, "Texture2D Tex0 : register(t0);\n");

		WRITE(p,"void main(\n");
		WRITE(p,"  out float4 ocol0 : SV_Target)\n");
	}

	WRITE(p, "{\n"
	"  int2 sampleUv;\n"
	"  int2 uv1 = int2(gl_FragCoord.xy);\n");

	WRITE(p, "  uv1.x = uv1.x * %d;\n", samples);

	WRITE(p, "  int yl = uv1.y / %d;\n", blkH);
	WRITE(p, "  int yb = yl * %d;\n", blkH);
	WRITE(p, "  int yoff = uv1.y - yb;\n");
	WRITE(p, "  int xp = uv1.x + yoff * int(" I_COLORS".z);\n");
	WRITE(p, "  int xel = xp / %d;\n", samples == 1 ? factor : blkW);
	WRITE(p, "  int xb = xel / %d;\n", blkH);
	WRITE(p, "  int xoff = xel - xb * %d;\n", blkH);
	WRITE(p, "  int xl =  uv1.x * %d / %d;\n", factor, blkW);
	WRITE(p, "  int xib = uv1.x * %d - xl * %d;\n", factor, blkW);
	WRITE(p, "  int halfxb = xb / %d;\n", factor);

	WRITE(p, "  sampleUv.x = xib + halfxb * %d;\n", blkW);
	WRITE(p, "  sampleUv.y = yb + xoff;\n");
}

void WriteSampleColor(char*& p, const char* colorComp, const char* dest, API_TYPE ApiType)
{
	WRITE(p,
		"{\n"                                          // sampleUv is the sample position in (int)gx_coords
		"float2 uv = float2(sampleUv) + int2(%d,0);\n" // pixel offset (if more than one pixel is samped)
		"uv *= " I_COLORS".w;\n"                       // scale by two (if wanted)
		"uv += " I_COLORS".xy;\n"                      // move to copyed rect
		"uv += float2(0.5, 0.5);\n"                    // move to center of pixel
		"uv /= float2(%d, %d);\n"                      // normlize to [0:1]
		"uv.y = 1.0-uv.y;\n"                           // ogl foo (disable this line for d3d)
		"%s = texture(samp0, uv).%s;\n"
		"}\n",
		s_incrementSampleXCount, EFB_WIDTH, EFB_HEIGHT, dest, colorComp
	);
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
	WRITE(p, "  %s = floor(%s * 255.0 / exp2(8.0 - %d.0));\n", dest, src, depth);
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

	WRITE(p, "  ocol0 = (color0 * 16.0 + color1) / 255.0;\n");
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

	WRITE(p, "  ocol0 = (color0 * 16.0 + color1) / 255.0;\n");
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
	WRITE(p, "  float2 gUpper = floor(gInt / 8.0);\n");
	WRITE(p, "  float2 gLower = gInt - gUpper * 8.0;\n");

	WriteToBitDepth(p, 5, "texRs", "ocol0.br");
	WRITE(p, "  ocol0.br = ocol0.br * 8.0 + gUpper;\n");
	WriteToBitDepth(p, 5, "texBs", "ocol0.ga");
	WRITE(p, "  ocol0.ga = ocol0.ga + gLower * 32.0;\n");

	WRITE(p, "  ocol0 = ocol0 / 255.0;\n");
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
	WRITE(p, "  gUpper = floor(color0 / 8.0);\n");
	WRITE(p, "  gLower = color0 - gUpper * 8.0;\n");

	WriteToBitDepth(p, 5, "texSample.r", "ocol0.b");
	WRITE(p, "  ocol0.b = ocol0.b * 4.0 + gUpper + 128.0;\n");
	WriteToBitDepth(p, 5, "texSample.b", "ocol0.g");
	WRITE(p, "  ocol0.g = ocol0.g + gLower * 32.0;\n");

	WRITE(p, "} else {\n");

	WriteToBitDepth(p, 4, "texSample.r", "ocol0.b");
	WriteToBitDepth(p, 4, "texSample.b", "ocol0.g");

	WriteToBitDepth(p, 3, "texSample.a", "color0");
	WRITE(p, "ocol0.b = ocol0.b + color0 * 16.0;\n");
	WriteToBitDepth(p, 4, "texSample.g", "color0");
	WRITE(p, "ocol0.g = ocol0.g + color0 * 16.0;\n");

	WRITE(p, "}\n");


	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "rgba", "texSample", ApiType);

	WRITE(p, "if(texSample.a > 0.878f) {\n");

	WriteToBitDepth(p, 5, "texSample.g", "color0");
	WRITE(p, "  gUpper = floor(color0 / 8.0);\n");
	WRITE(p, "  gLower = color0 - gUpper * 8.0;\n");

	WriteToBitDepth(p, 5, "texSample.r", "ocol0.r");
	WRITE(p, "  ocol0.r = ocol0.r * 4.0 + gUpper + 128.0;\n");
	WriteToBitDepth(p, 5, "texSample.b", "ocol0.a");
	WRITE(p, "  ocol0.a = ocol0.a + gLower * 32.0;\n");

	WRITE(p, "} else {\n");

	WriteToBitDepth(p, 4, "texSample.r", "ocol0.r");
	WriteToBitDepth(p, 4, "texSample.b", "ocol0.a");

	WriteToBitDepth(p, 3, "texSample.a", "color0");
	WRITE(p, "ocol0.r = ocol0.r + color0 * 16.0;\n");
	WriteToBitDepth(p, 4, "texSample.g", "color0");
	WRITE(p, "ocol0.a = ocol0.a + color0 * 16.0;\n");

	WRITE(p, "}\n");

	WRITE(p, "  ocol0 = ocol0 / 255.0;\n");
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

	WRITE(p, "  ocol0 = (color0 * 16.0 + color1) / 255.0;\n");
	WriteEncoderEnd(p, ApiType);
}

void WriteRGBA8Encoder(char* p,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_TF_RGBA8, ApiType);

	WRITE(p, "  float cl1 = xb - (halfxb * 2.0);\n");
	WRITE(p, "  float cl0 = 1.0 - cl1;\n");

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

	WRITE(p, "  ocol0 = (color0 * 16.0 + color1) / 255.0;\n");
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

	WRITE(p, "  ocol0 = (color0 * 16.0 + color1) / 255.0;\n");
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

	WRITE(p, "  depth *= 16777215.0;\n");
	WRITE(p, "  expanded.r = floor(depth / (256.0 * 256.0));\n");
	WRITE(p, "  depth -= expanded.r * 256.0 * 256.0;\n");
	WRITE(p, "  expanded.g = floor(depth / 256.0);\n");

	WRITE(p, "  ocol0.b = expanded.g / 255.0;\n");
	WRITE(p, "  ocol0.g = expanded.r / 255.0;\n");

	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "b", "depth", ApiType);

	WRITE(p, "  depth *= 16777215.0;\n");
	WRITE(p, "  expanded.r = floor(depth / (256.0 * 256.0));\n");
	WRITE(p, "  depth -= expanded.r * 256.0 * 256.0;\n");
	WRITE(p, "  expanded.g = floor(depth / 256.0);\n");

	WRITE(p, "  ocol0.r = expanded.g / 255.0;\n");
	WRITE(p, "  ocol0.a = expanded.r / 255.0;\n");

	WriteEncoderEnd(p, ApiType);
}

void WriteZ16LEncoder(char* p,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_CTF_Z16L, ApiType);

	WRITE(p, "  float depth;\n");
	WRITE(p, "  float3 expanded;\n");

	// byte order is reversed

	WriteSampleColor(p, "b", "depth", ApiType);

	WRITE(p, "  depth *= 16777215.0;\n");
	WRITE(p, "  expanded.r = floor(depth / (256.0 * 256.0));\n");
	WRITE(p, "  depth -= expanded.r * 256.0 * 256.0;\n");
	WRITE(p, "  expanded.g = floor(depth / 256.0);\n");
	WRITE(p, "  depth -= expanded.g * 256.0;\n");
	WRITE(p, "  expanded.b = depth;\n");

	WRITE(p, "  ocol0.b = expanded.b / 255.0;\n");
	WRITE(p, "  ocol0.g = expanded.g / 255.0;\n");

	WriteIncrementSampleX(p, ApiType);

	WriteSampleColor(p, "b", "depth", ApiType);

	WRITE(p, "  depth *= 16777215.0;\n");
	WRITE(p, "  expanded.r = floor(depth / (256.0 * 256.0));\n");
	WRITE(p, "  depth -= expanded.r * 256.0 * 256.0;\n");
	WRITE(p, "  expanded.g = floor(depth / 256.0);\n");
	WRITE(p, "  depth -= expanded.g * 256.0;\n");
	WRITE(p, "  expanded.b = depth;\n");

	WRITE(p, "  ocol0.r = expanded.b;\n");
	WRITE(p, "  ocol0.a = expanded.g;\n");

	WriteEncoderEnd(p, ApiType);
}

void WriteZ24Encoder(char* p, API_TYPE ApiType)
{
	WriteSwizzler(p, GX_TF_Z24X8, ApiType);

	WRITE(p, "  float cl = xb - (halfxb * 2.0);\n");

	WRITE(p, "  float depth0;\n");
	WRITE(p, "  float depth1;\n");
	WRITE(p, "  float3 expanded0;\n");
	WRITE(p, "  float3 expanded1;\n");

	WriteSampleColor(p, "b", "depth0", ApiType);
	WriteIncrementSampleX(p, ApiType);
	WriteSampleColor(p, "b", "depth1", ApiType);

	for (int i = 0; i < 2; i++)
	{
		WRITE(p, "  depth%i *= 16777215.0;\n", i);

		WRITE(p, "  expanded%i.r = floor(depth%i / (256.0 * 256.0));\n", i, i);
		WRITE(p, "  depth%i -= expanded%i.r * 256.0 * 256.0;\n", i, i);
		WRITE(p, "  expanded%i.g = floor(depth%i / 256.0);\n", i, i);
		WRITE(p, "  depth%i -= expanded%i.g * 256.0;\n", i, i);
		WRITE(p, "  expanded%i.b = depth%i;\n", i, i);
	}

	WRITE(p, "  if(cl > 0.5) {\n");
	// upper 16
	WRITE(p, "     ocol0.b = expanded0.g / 255.0;\n");
	WRITE(p, "     ocol0.g = expanded0.b / 255.0;\n");
	WRITE(p, "     ocol0.r = expanded1.g / 255.0;\n");
	WRITE(p, "     ocol0.a = expanded1.b / 255.0;\n");
	WRITE(p, "  } else {\n");
	// lower 8
	WRITE(p, "     ocol0.b = 1.0;\n");
	WRITE(p, "     ocol0.g = expanded0.r / 255.0;\n");
	WRITE(p, "     ocol0.r = 1.0;\n");
	WRITE(p, "     ocol0.a = expanded1.r / 255.0;\n");
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
		WriteZ8Encoder(p, "256.0", ApiType);
		break;
	case GX_CTF_Z8L:
		WriteZ8Encoder(p, "65536.0" , ApiType);
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

}  // namespace
