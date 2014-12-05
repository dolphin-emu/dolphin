// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>
#include <cstdio>
#include <locale.h>
#ifdef __APPLE__
	#include <xlocale.h>
#endif

#include "Common/MathUtil.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/TextureConversionShader.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoConfig.h"

#define WRITE p+=sprintf

static char text[16384];
static bool IntensityConstantAdded =  false;

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
static void WriteSwizzler(char*& p, u32 format, API_TYPE ApiType)
{
	// left, top, of source rectangle within source texture
	// width of the destination rectangle, scale_factor (1 or 2)
	WRITE(p, "uniform int4 position;\n");

	int blkW = TexDecoder_GetBlockWidthInTexels(format);
	int blkH = TexDecoder_GetBlockHeightInTexels(format);
	int samples = GetEncodedSampleCount(format);

	if (ApiType == API_OPENGL)
	{
		WRITE(p, "#define samp0 samp9\n");
		WRITE(p, "SAMPLER_BINDING(9) uniform sampler2DArray samp0;\n");

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
	"  int2 uv1 = int2(gl_FragCoord.xy);\n"
	);

	WRITE(p, "  int y_block_position = uv1.y & %d;\n", ~(blkH - 1));
	WRITE(p, "  int y_offset_in_block = uv1.y & %d;\n", blkH - 1);
	WRITE(p, "  int x_virtual_position = (uv1.x << %d) + y_offset_in_block * position.z;\n", IntLog2(samples));
	WRITE(p, "  int x_block_position = (x_virtual_position >> %d) & %d;\n", IntLog2(blkH), ~(blkW - 1));
	if (samples == 1)
	{
		// 32 bit textures (RGBA8 and Z24) are stored in 2 cache line increments
		WRITE(p, "  bool first = 0 == (x_virtual_position & %d);\n", 8 * samples); // first cache line, used in the encoders
		WRITE(p, "  x_virtual_position = x_virtual_position << 1;\n");
	}
	WRITE(p, "  int x_offset_in_block = x_virtual_position & %d;\n", blkW - 1);
	WRITE(p, "  int y_offset = (x_virtual_position >> %d) & %d;\n", IntLog2(blkW), blkH - 1);

	WRITE(p, "  sampleUv.x = x_offset_in_block + x_block_position;\n");
	WRITE(p, "  sampleUv.y = y_block_position + y_offset;\n");

	WRITE(p, "  float2 uv0 = float2(sampleUv);\n");                // sampleUv is the sample position in (int)gx_coords
	WRITE(p, "  uv0 += float2(0.5, 0.5);\n");                      // move to center of pixel
	WRITE(p, "  uv0 *= float(position.w);\n");                     // scale by two if needed (also move to pixel borders so that linear filtering will average adjacent pixel)
	WRITE(p, "  uv0 += float2(position.xy);\n");                   // move to copied rect
	WRITE(p, "  uv0 /= float2(%d, %d);\n", EFB_WIDTH, EFB_HEIGHT); // normalize to [0:1]
	if (ApiType == API_OPENGL)                                     // ogl has to flip up and down
	{
		WRITE(p, "  uv0.y = 1.0-uv0.y;\n");
	}

	WRITE(p, "  float sample_offset = float(position.w) / float(%d);\n", EFB_WIDTH);
}

static void WriteSampleColor(char*& p, const char* colorComp, const char* dest, int xoffset, API_TYPE ApiType)
{
	WRITE(p, "  %s = texture(samp0, float3(uv0 + float2(%d, 0) * sample_offset, 0.0)).%s;\n",
		dest, xoffset, colorComp
	);
}

static void WriteColorToIntensity(char*& p, const char* src, const char* dest)
{
	if (!IntensityConstantAdded)
	{
		WRITE(p, "  float4 IntensityConst = float4(0.257f,0.504f,0.098f,0.0625f);\n");
		IntensityConstantAdded = true;
	}
	WRITE(p, "  %s = dot(IntensityConst.rgb, %s.rgb);\n", dest, src);
	// don't add IntensityConst.a yet, because doing it later is faster and uses less instructions, due to vectorization
}

static void WriteToBitDepth(char*& p, u8 depth, const char* src, const char* dest)
{
	WRITE(p, "  %s = floor(%s * 255.0 / exp2(8.0 - %d.0));\n", dest, src, depth);
}

static void WriteEncoderEnd(char*& p, API_TYPE ApiType)
{
	WRITE(p, "}\n");
	IntensityConstantAdded = false;
}

static void WriteI8Encoder(char*& p, API_TYPE ApiType)
{
	WriteSwizzler(p, GX_TF_I8, ApiType);
	WRITE(p, "  float3 texSample;\n");

	WriteSampleColor(p, "rgb", "texSample", 0, ApiType);
	WriteColorToIntensity(p, "texSample", "ocol0.b");

	WriteSampleColor(p, "rgb", "texSample", 1, ApiType);
	WriteColorToIntensity(p, "texSample", "ocol0.g");

	WriteSampleColor(p, "rgb", "texSample", 2, ApiType);
	WriteColorToIntensity(p, "texSample", "ocol0.r");

	WriteSampleColor(p, "rgb", "texSample", 3, ApiType);
	WriteColorToIntensity(p, "texSample", "ocol0.a");

	WRITE(p, "  ocol0.rgba += IntensityConst.aaaa;\n"); // see WriteColorToIntensity

	WriteEncoderEnd(p, ApiType);
}

static void WriteI4Encoder(char*& p, API_TYPE ApiType)
{
	WriteSwizzler(p, GX_TF_I4, ApiType);
	WRITE(p, "  float3 texSample;\n");
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, "rgb", "texSample", 0, ApiType);
	WriteColorToIntensity(p, "texSample", "color0.b");

	WriteSampleColor(p, "rgb", "texSample", 1, ApiType);
	WriteColorToIntensity(p, "texSample", "color1.b");

	WriteSampleColor(p, "rgb", "texSample", 2, ApiType);
	WriteColorToIntensity(p, "texSample", "color0.g");

	WriteSampleColor(p, "rgb", "texSample", 3, ApiType);
	WriteColorToIntensity(p, "texSample", "color1.g");

	WriteSampleColor(p, "rgb", "texSample", 4, ApiType);
	WriteColorToIntensity(p, "texSample", "color0.r");

	WriteSampleColor(p, "rgb", "texSample", 5, ApiType);
	WriteColorToIntensity(p, "texSample", "color1.r");

	WriteSampleColor(p, "rgb", "texSample", 6, ApiType);
	WriteColorToIntensity(p, "texSample", "color0.a");

	WriteSampleColor(p, "rgb", "texSample", 7, ApiType);
	WriteColorToIntensity(p, "texSample", "color1.a");

	WRITE(p, "  color0.rgba += IntensityConst.aaaa;\n");
	WRITE(p, "  color1.rgba += IntensityConst.aaaa;\n");

	WriteToBitDepth(p, 4, "color0", "color0");
	WriteToBitDepth(p, 4, "color1", "color1");

	WRITE(p, "  ocol0 = (color0 * 16.0 + color1) / 255.0;\n");
	WriteEncoderEnd(p, ApiType);
}

static void WriteIA8Encoder(char*& p,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_TF_IA8, ApiType);
	WRITE(p, "  float4 texSample;\n");

	WriteSampleColor(p, "rgba", "texSample", 0, ApiType);
	WRITE(p, "  ocol0.b = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "ocol0.g");

	WriteSampleColor(p, "rgba", "texSample", 1, ApiType);
	WRITE(p, "  ocol0.r = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "ocol0.a");

	WRITE(p, "  ocol0.ga += IntensityConst.aa;\n");

	WriteEncoderEnd(p, ApiType);
}

static void WriteIA4Encoder(char*& p,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_TF_IA4, ApiType);
	WRITE(p, "  float4 texSample;\n");
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, "rgba", "texSample", 0, ApiType);
	WRITE(p, "  color0.b = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "color1.b");

	WriteSampleColor(p, "rgba", "texSample", 1, ApiType);
	WRITE(p, "  color0.g = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "color1.g");

	WriteSampleColor(p, "rgba", "texSample", 2, ApiType);
	WRITE(p, "  color0.r = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "color1.r");

	WriteSampleColor(p, "rgba", "texSample", 3, ApiType);
	WRITE(p, "  color0.a = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "color1.a");

	WRITE(p, "  color1.rgba += IntensityConst.aaaa;\n");

	WriteToBitDepth(p, 4, "color0", "color0");
	WriteToBitDepth(p, 4, "color1", "color1");

	WRITE(p, "  ocol0 = (color0 * 16.0 + color1) / 255.0;\n");
	WriteEncoderEnd(p, ApiType);
}

static void WriteRGB565Encoder(char*& p,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_TF_RGB565, ApiType);

	WriteSampleColor(p, "rgb", "float3 texSample0", 0, ApiType);
	WriteSampleColor(p, "rgb", "float3 texSample1", 1, ApiType);
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

static void WriteRGB5A3Encoder(char*& p,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_TF_RGB5A3, ApiType);

	WRITE(p, "  float4 texSample;\n");
	WRITE(p, "  float color0;\n");
	WRITE(p, "  float gUpper;\n");
	WRITE(p, "  float gLower;\n");

	WriteSampleColor(p, "rgba", "texSample", 0, ApiType);

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



	WriteSampleColor(p, "rgba", "texSample", 1, ApiType);

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

static void WriteRGBA8Encoder(char*& p,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_TF_RGBA8, ApiType);

	WRITE(p, "  float4 texSample;\n");
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, "rgba", "texSample", 0, ApiType);
	WRITE(p, "  color0.b = texSample.a;\n");
	WRITE(p, "  color0.g = texSample.r;\n");
	WRITE(p, "  color1.b = texSample.g;\n");
	WRITE(p, "  color1.g = texSample.b;\n");

	WriteSampleColor(p, "rgba", "texSample", 1, ApiType);
	WRITE(p, "  color0.r = texSample.a;\n");
	WRITE(p, "  color0.a = texSample.r;\n");
	WRITE(p, "  color1.r = texSample.g;\n");
	WRITE(p, "  color1.a = texSample.b;\n");

	WRITE(p, "  ocol0 = first ? color0 : color1;\n");

	WriteEncoderEnd(p, ApiType);
}

static void WriteC4Encoder(char*& p, const char* comp,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_CTF_R4, ApiType);
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, comp, "color0.b", 0, ApiType);
	WriteSampleColor(p, comp, "color1.b", 1, ApiType);
	WriteSampleColor(p, comp, "color0.g", 2, ApiType);
	WriteSampleColor(p, comp, "color1.g", 3, ApiType);
	WriteSampleColor(p, comp, "color0.r", 4, ApiType);
	WriteSampleColor(p, comp, "color1.r", 5, ApiType);
	WriteSampleColor(p, comp, "color0.a", 6, ApiType);
	WriteSampleColor(p, comp, "color1.a", 7, ApiType);

	WriteToBitDepth(p, 4, "color0", "color0");
	WriteToBitDepth(p, 4, "color1", "color1");

	WRITE(p, "  ocol0 = (color0 * 16.0 + color1) / 255.0;\n");
	WriteEncoderEnd(p, ApiType);
}

static void WriteC8Encoder(char*& p, const char* comp,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_CTF_R8, ApiType);

	WriteSampleColor(p, comp, "ocol0.b", 0, ApiType);
	WriteSampleColor(p, comp, "ocol0.g", 1, ApiType);
	WriteSampleColor(p, comp, "ocol0.r", 2, ApiType);
	WriteSampleColor(p, comp, "ocol0.a", 3, ApiType);

	WriteEncoderEnd(p, ApiType);
}

static void WriteCC4Encoder(char*& p, const char* comp,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_CTF_RA4, ApiType);
	WRITE(p, "  float2 texSample;\n");
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, comp, "texSample", 0, ApiType);
	WRITE(p, "  color0.b = texSample.x;\n");
	WRITE(p, "  color1.b = texSample.y;\n");

	WriteSampleColor(p, comp, "texSample", 1, ApiType);
	WRITE(p, "  color0.g = texSample.x;\n");
	WRITE(p, "  color1.g = texSample.y;\n");

	WriteSampleColor(p, comp, "texSample", 2, ApiType);
	WRITE(p, "  color0.r = texSample.x;\n");
	WRITE(p, "  color1.r = texSample.y;\n");

	WriteSampleColor(p, comp, "texSample", 3, ApiType);
	WRITE(p, "  color0.a = texSample.x;\n");
	WRITE(p, "  color1.a = texSample.y;\n");

	WriteToBitDepth(p, 4, "color0", "color0");
	WriteToBitDepth(p, 4, "color1", "color1");

	WRITE(p, "  ocol0 = (color0 * 16.0 + color1) / 255.0;\n");
	WriteEncoderEnd(p, ApiType);
}

static void WriteCC8Encoder(char*& p, const char* comp, API_TYPE ApiType)
{
	WriteSwizzler(p, GX_CTF_RA8, ApiType);

	WriteSampleColor(p, comp, "ocol0.bg", 0, ApiType);
	WriteSampleColor(p, comp, "ocol0.ra", 1, ApiType);

	WriteEncoderEnd(p, ApiType);
}

static void WriteZ8Encoder(char*& p, const char* multiplier,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_CTF_Z8M, ApiType);

	WRITE(p, " float depth;\n");

	WriteSampleColor(p, "b", "depth", 0, ApiType);
	WRITE(p, "ocol0.b = frac(depth * %s);\n", multiplier);

	WriteSampleColor(p, "b", "depth", 1, ApiType);
	WRITE(p, "ocol0.g = frac(depth * %s);\n", multiplier);

	WriteSampleColor(p, "b", "depth", 2, ApiType);
	WRITE(p, "ocol0.r = frac(depth * %s);\n", multiplier);

	WriteSampleColor(p, "b", "depth", 3, ApiType);
	WRITE(p, "ocol0.a = frac(depth * %s);\n", multiplier);

	WriteEncoderEnd(p, ApiType);
}

static void WriteZ16Encoder(char*& p,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_TF_Z16, ApiType);

	WRITE(p, "  float depth;\n");
	WRITE(p, "  float3 expanded;\n");

	// byte order is reversed

	WriteSampleColor(p, "b", "depth", 0, ApiType);

	WRITE(p, "  depth *= 16777215.0;\n");
	WRITE(p, "  expanded.r = floor(depth / (256.0 * 256.0));\n");
	WRITE(p, "  depth -= expanded.r * 256.0 * 256.0;\n");
	WRITE(p, "  expanded.g = floor(depth / 256.0);\n");

	WRITE(p, "  ocol0.b = expanded.g / 255.0;\n");
	WRITE(p, "  ocol0.g = expanded.r / 255.0;\n");

	WriteSampleColor(p, "b", "depth", 1, ApiType);

	WRITE(p, "  depth *= 16777215.0;\n");
	WRITE(p, "  expanded.r = floor(depth / (256.0 * 256.0));\n");
	WRITE(p, "  depth -= expanded.r * 256.0 * 256.0;\n");
	WRITE(p, "  expanded.g = floor(depth / 256.0);\n");

	WRITE(p, "  ocol0.r = expanded.g / 255.0;\n");
	WRITE(p, "  ocol0.a = expanded.r / 255.0;\n");

	WriteEncoderEnd(p, ApiType);
}

static void WriteZ16LEncoder(char*& p,API_TYPE ApiType)
{
	WriteSwizzler(p, GX_CTF_Z16L, ApiType);

	WRITE(p, "  float depth;\n");
	WRITE(p, "  float3 expanded;\n");

	// byte order is reversed

	WriteSampleColor(p, "b", "depth", 0, ApiType);

	WRITE(p, "  depth *= 16777215.0;\n");
	WRITE(p, "  expanded.r = floor(depth / (256.0 * 256.0));\n");
	WRITE(p, "  depth -= expanded.r * 256.0 * 256.0;\n");
	WRITE(p, "  expanded.g = floor(depth / 256.0);\n");
	WRITE(p, "  depth -= expanded.g * 256.0;\n");
	WRITE(p, "  expanded.b = depth;\n");

	WRITE(p, "  ocol0.b = expanded.b / 255.0;\n");
	WRITE(p, "  ocol0.g = expanded.g / 255.0;\n");

	WriteSampleColor(p, "b", "depth", 1, ApiType);

	WRITE(p, "  depth *= 16777215.0;\n");
	WRITE(p, "  expanded.r = floor(depth / (256.0 * 256.0));\n");
	WRITE(p, "  depth -= expanded.r * 256.0 * 256.0;\n");
	WRITE(p, "  expanded.g = floor(depth / 256.0);\n");
	WRITE(p, "  depth -= expanded.g * 256.0;\n");
	WRITE(p, "  expanded.b = depth;\n");

	WRITE(p, "  ocol0.r = expanded.b / 255.0;\n");
	WRITE(p, "  ocol0.a = expanded.g / 255.0;\n");

	WriteEncoderEnd(p, ApiType);
}

static void WriteZ24Encoder(char*& p, API_TYPE ApiType)
{
	WriteSwizzler(p, GX_TF_Z24X8, ApiType);

	WRITE(p, "  float depth0;\n");
	WRITE(p, "  float depth1;\n");
	WRITE(p, "  float3 expanded0;\n");
	WRITE(p, "  float3 expanded1;\n");

	WriteSampleColor(p, "b", "depth0", 0, ApiType);
	WriteSampleColor(p, "b", "depth1", 1, ApiType);

	for (int i = 0; i < 2; i++)
	{
		WRITE(p, "  depth%i *= 16777215.0;\n", i);

		WRITE(p, "  expanded%i.r = floor(depth%i / (256.0 * 256.0));\n", i, i);
		WRITE(p, "  depth%i -= expanded%i.r * 256.0 * 256.0;\n", i, i);
		WRITE(p, "  expanded%i.g = floor(depth%i / 256.0);\n", i, i);
		WRITE(p, "  depth%i -= expanded%i.g * 256.0;\n", i, i);
		WRITE(p, "  expanded%i.b = depth%i;\n", i, i);
	}

	WRITE(p, "  if (!first) {\n");
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
	locale_t locale = newlocale(LC_NUMERIC_MASK, "C", nullptr); // New locale for compilation
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
