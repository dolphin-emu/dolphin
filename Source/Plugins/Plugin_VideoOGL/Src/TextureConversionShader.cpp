// Copyright (C) 2003-2000 Dolphin Project.

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


#include "TextureConversionShader.h"
#include "TextureDecoder.h"
#include "PixelShaderManager.h"
#include "PixelShaderGen.h"
#include "BPMemory.h"

#include <stdio.h>
#include <math.h>
#include "Common.h"

#define WRITE p+=sprintf

static char text[16384];

namespace TextureConversionShader
{

u16 GetBlockWidthInTexels(u32 format)
{
    switch (format) {
    case GX_TF_I4: return 8;   
    case GX_TF_I8: return 8;
    case GX_TF_IA4: return 8;
    case GX_TF_IA8: return 4;
    case GX_TF_RGB565: return 4;
    case GX_TF_RGB5A3: return 4;
    case GX_TF_RGBA8: return 4;
	case GX_CTF_R4: return 8;
    case GX_CTF_RA4: return 8;
    case GX_CTF_RA8: return 4;
    case GX_CTF_A8: return 8;
    case GX_CTF_R8: return 8;
    case GX_CTF_G8: return 8;
    case GX_CTF_B8: return 8;
    case GX_CTF_RG8: return 4;
    case GX_CTF_GB8: return 4;
	case GX_TF_Z8: return 8;
	case GX_TF_Z16: return 4;
	case GX_TF_Z24X8: return 4;
	case GX_CTF_Z4: return 8;
	case GX_CTF_Z8M: return 8;
	case GX_CTF_Z8L: return 8;
	case GX_CTF_Z16L: return 4;
    default: return 8;
    }
}

u16 GetBlockHeightInTexels(u32 format)
{
    switch (format) {    
	case GX_TF_I4: return 8; 
    case GX_TF_I8: return 4;
	case GX_TF_IA4: return 4; 
    case GX_TF_IA8: return 4;
	case GX_TF_RGB565: return 4;
	case GX_TF_RGB5A3: return 4;
	case GX_TF_RGBA8: return 4;
	case GX_CTF_R4: return 8;
    case GX_CTF_RA4: return 4;
    case GX_CTF_RA8: return 4;
    case GX_CTF_A8: return 4;
    case GX_CTF_R8: return 4;
    case GX_CTF_G8: return 4;
    case GX_CTF_B8: return 4;
    case GX_CTF_RG8: return 4;
    case GX_CTF_GB8: return 4;
	case GX_TF_Z8: return 4;
	case GX_TF_Z16: return 4;
	case GX_TF_Z24X8: return 4;
	case GX_CTF_Z4: return 8;
	case GX_CTF_Z8M: return 4;
	case GX_CTF_Z8L: return 4;
	case GX_CTF_Z16L: return 4;
    default: return 8;
    }
}

u16 GetEncodedSampleCount(u32 format)
{
    switch (format) {    
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
void WriteSwizzler(char*& p, u32 format)
{
	WRITE(p, "uniform float4 blkDims : register(c%d);\n", C_COLORMATRIX);
	WRITE(p, "uniform float4 textureDims : register(c%d);\n", C_COLORMATRIX + 1);

    float blkW = (float)GetBlockWidthInTexels(format);
	float blkH = (float)GetBlockHeightInTexels(format);
	float samples = (float)GetEncodedSampleCount(format);

	WRITE(p, 
	"uniform samplerRECT samp0 : register(s0);\n"
	"void main(\n"
	"  out float4 ocol0 : COLOR0,\n"
	"  in float2 uv0 : TEXCOORD0)\n"
	"{\n"    
    "  float2 sampleUv;\n"
	"  float2 uv1 = floor(uv0);\n");

	WRITE(p, "  uv1.x = uv1.x * %f;\n", samples);

	WRITE(p, "  float xl = floor(uv1.x / %f);\n", blkW);
	WRITE(p, "  float xib = uv1.x - (xl * %f);\n", blkW);
	WRITE(p, "  float yl = floor(uv1.y / %f);\n", blkH);
	WRITE(p, "  float yb = yl * %f;\n", blkH);
	WRITE(p, "  float yoff = uv1.y - yb;\n");
	WRITE(p, "  float xp = uv1.x + (yoff * textureDims.x);\n");
	WRITE(p, "  float xel = floor(xp / %f);\n", blkW);
	WRITE(p, "  float xb = floor(xel / %f);\n", blkH);
	WRITE(p, "  float xoff = xel - (xb * %f);\n", blkH);

	WRITE(p, "  sampleUv.x = xib + (xb * %f);\n", blkW);
	WRITE(p, "  sampleUv.y = yb + xoff;\n");

	WRITE(p, "  sampleUv = sampleUv * blkDims.xy;\n"
	         "  sampleUv.y = textureDims.y - sampleUv.y;\n"
	
	         "  sampleUv.x = sampleUv.x + textureDims.z;\n"
	         "  sampleUv.y = sampleUv.y + textureDims.w;\n");
}

// block dimensions : widthStride, heightStride 
// texture dims : width, height, x offset, y offset
void Write32BitSwizzler(char*& p, u32 format)
{
    WRITE(p, "uniform float4 blkDims : register(c%d);\n", C_COLORMATRIX);
	WRITE(p, "uniform float4 textureDims : register(c%d);\n", C_COLORMATRIX + 1);

    float blkW = (float)GetBlockWidthInTexels(format);
	float blkH = (float)GetBlockHeightInTexels(format);

	// 32 bit textures (RGBA8 and Z24) are store in 2 cache line increments

	WRITE(p, 
	"uniform samplerRECT samp0 : register(s0);\n"
	"void main(\n"
	"  out float4 ocol0 : COLOR0,\n"
	"  in float2 uv0 : TEXCOORD0)\n"
	"{\n"
    "  float2 sampleUv;\n"
	"  float2 uv1 = floor(uv0);\n");
	
	WRITE(p, "  float yl = floor(uv1.y / %f);\n", blkH);
	WRITE(p, "  float yb = yl * %f;\n", blkH);
	WRITE(p, "  float yoff = uv1.y - yb;\n");
	WRITE(p, "  float xp = uv1.x + (yoff * textureDims.x);\n");
	WRITE(p, "  float xel = floor(xp / 2);\n");
	WRITE(p, "  float xb = floor(xel / %f);\n", blkH);
	WRITE(p, "  float xoff = xel - (xb * %f);\n", blkH);
	
	WRITE(p, "  float x2 = uv1.x * 2;\n");
	WRITE(p, "  float xl = floor(x2 / %f);\n", blkW);	
	WRITE(p, "  float xib = x2 - (xl * %f);\n", blkW);
	WRITE(p, "  float halfxb = floor(xb / 2);\n");

	
	WRITE(p, "  sampleUv.x = xib + (halfxb * %f);\n", blkW);
	WRITE(p, "  sampleUv.y = yb + xoff;\n");
	WRITE(p, "  sampleUv = sampleUv * blkDims.xy;\n");
	WRITE(p, "  sampleUv.y = textureDims.y - sampleUv.y;\n");
	
	WRITE(p, "  sampleUv.x = sampleUv.x + textureDims.z;\n");
	WRITE(p, "  sampleUv.y = sampleUv.y + textureDims.w;\n");
}

void WriteSampleColor(char*& p, const char* colorComp, const char* dest)
{
	WRITE(p, "  %s = texRECT(samp0, sampleUv).%s;\n", dest, colorComp);
}

void WriteColorToIntensity(char*& p, const char* src, const char* dest)
{
	WRITE(p, "  %s = (0.257f * %s.r) + (0.504f * %s.g) + (0.098f * %s.b) + 0.0625f;\n", dest, src, src, src);
}

void WriteIncrementSampleX(char*& p)
{
	WRITE(p, "  sampleUv.x = sampleUv.x + blkDims.x;\n");
}

void WriteToBitDepth(char*& p, u8 depth, const char* src, const char* dest)
{
	float result = pow(2.0f, depth) - 1.0f;
	WRITE(p, "  %s = floor(%s * %ff);\n", dest, src, result);
}

void WriteI8Encoder(char* p)
{
	WriteSwizzler(p, GX_TF_I8);
	WRITE(p, "  float3 texSample;\n");	

	WriteSampleColor(p, "rgb", "texSample");
	WriteColorToIntensity(p, "texSample", "ocol0.b");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, "rgb", "texSample");
	WriteColorToIntensity(p, "texSample", "ocol0.g");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, "rgb", "texSample");
	WriteColorToIntensity(p, "texSample", "ocol0.r");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, "rgb", "texSample");
	WriteColorToIntensity(p, "texSample", "ocol0.a");

	WRITE(p, "}\n");
}

void WriteI4Encoder(char* p)
{
	WriteSwizzler(p, GX_TF_I4);
	WRITE(p, "  float3 texSample;\n");
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, "rgb", "texSample");
	WriteColorToIntensity(p, "texSample", "color0.b");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, "rgb", "texSample");
	WriteColorToIntensity(p, "texSample", "color1.b");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, "rgb", "texSample");
	WriteColorToIntensity(p, "texSample", "color0.g");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, "rgb", "texSample");
	WriteColorToIntensity(p, "texSample", "color1.g");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, "rgb", "texSample");
	WriteColorToIntensity(p, "texSample", "color0.r");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, "rgb", "texSample");
	WriteColorToIntensity(p, "texSample", "color1.r");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, "rgb", "texSample");
	WriteColorToIntensity(p, "texSample", "color0.a");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, "rgb", "texSample");
	WriteColorToIntensity(p, "texSample", "color1.a");

	WriteToBitDepth(p, 4, "color0", "color0");
	WriteToBitDepth(p, 4, "color1", "color1");

	WRITE(p, "  ocol0 = (color0 * 16.0f + color1) / 255.0f;\n");
	WRITE(p, "}\n");
}

void WriteIA8Encoder(char* p)
{
	WriteSwizzler(p, GX_TF_IA8);
	WRITE(p, "  float4 texSample;\n");	

	WriteSampleColor(p, "rgba", "texSample");
	WRITE(p, "  ocol0.b = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "ocol0.g");
	WriteIncrementSampleX(p);	

	WriteSampleColor(p, "rgba", "texSample");
	WRITE(p, "  ocol0.r = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "ocol0.a");

	WRITE(p, "}\n");
}

void WriteIA4Encoder(char* p)
{
	WriteSwizzler(p, GX_TF_IA4);
	WRITE(p, "  float4 texSample;\n");
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, "rgba", "texSample");
	WRITE(p, "  color0.b = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "color1.b");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, "rgba", "texSample");
	WRITE(p, "  color0.g = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "color1.g");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, "rgba", "texSample");
	WRITE(p, "  color0.r = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "color1.r");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, "rgba", "texSample");
	WRITE(p, "  color0.a = texSample.a;\n");
	WriteColorToIntensity(p, "texSample", "color1.a");

	WriteToBitDepth(p, 4, "color0", "color0");
	WriteToBitDepth(p, 4, "color1", "color1");

	WRITE(p, "  ocol0 = (color0 * 16.0f + color1) / 255.0f;\n");
	WRITE(p, "}\n");
}

void WriteRGB565Encoder(char* p)
{
	WriteSwizzler(p, GX_TF_RGB565);

	WRITE(p, "  float3 texSample;\n");
	WRITE(p, "  float gInt;\n");
	WRITE(p, "  float gUpper;\n");
	WRITE(p, "  float gLower;\n");

	WriteSampleColor(p, "rgb", "texSample");
	WriteToBitDepth(p, 6, "texSample.g", "gInt");
	WRITE(p, "  gUpper = floor(gInt / 8.0f);\n");
	WRITE(p, "  gLower = gInt - gUpper * 8.0f;\n");

	WriteToBitDepth(p, 5, "texSample.r", "ocol0.b");
	WRITE(p, "  ocol0.b = ocol0.b * 8.0f + gUpper;\n");
	WriteToBitDepth(p, 5, "texSample.b", "ocol0.g");
	WRITE(p, "  ocol0.g = ocol0.g + gLower * 32.0f;\n");

	WriteIncrementSampleX(p);

	WriteSampleColor(p, "rgb", "texSample");
	WriteToBitDepth(p, 6, "texSample.g", "gInt");
	WRITE(p, "  gUpper = floor(gInt / 8.0f);\n");
	WRITE(p, "  gLower = gInt - gUpper * 8.0f;\n");

	WriteToBitDepth(p, 5, "texSample.r", "ocol0.r");
	WRITE(p, "  ocol0.r = ocol0.r * 8.0f + gUpper;\n");
	WriteToBitDepth(p, 5, "texSample.b", "ocol0.a");
	WRITE(p, "  ocol0.a = ocol0.a + gLower * 32.0f;\n");

	WRITE(p, "  ocol0 = ocol0 / 255.0f;\n");
	WRITE(p, "}\n");
}

void WriteRGB5A3Encoder(char* p)
{
	WriteSwizzler(p, GX_TF_RGB5A3);

	WRITE(p, "  float4 texSample;\n");
	WRITE(p, "  float color0;\n");
	WRITE(p, "  float gUpper;\n");
	WRITE(p, "  float gLower;\n");

	WriteSampleColor(p, "rgba", "texSample");

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


	WriteIncrementSampleX(p);

	WriteSampleColor(p, "rgba", "texSample");

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
	WRITE(p, "}\n");
}

void WriteRGBA4443Encoder(char* p)
{
	WriteSwizzler(p, GX_TF_RGB5A3);

	WRITE(p, "  float4 texSample;\n");
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, "rgba", "texSample");
	WriteToBitDepth(p, 3, "texSample.a", "color0.b");
	WriteToBitDepth(p, 4, "texSample.r", "color1.b");
	WriteToBitDepth(p, 4, "texSample.g", "color0.g");
	WriteToBitDepth(p, 4, "texSample.b", "color1.g");

	WriteIncrementSampleX(p);

	WriteSampleColor(p, "rgba", "texSample");
	WriteToBitDepth(p, 3, "texSample.a", "color0.r");
	WriteToBitDepth(p, 4, "texSample.r", "color1.r");
	WriteToBitDepth(p, 4, "texSample.g", "color0.a");
	WriteToBitDepth(p, 4, "texSample.b", "color1.a");

	WRITE(p, "  ocol0 = (color0 * 16.0f + color1) / 255.0f;\n");
	WRITE(p, "}\n");
}

void WriteRGBA8Encoder(char* p)
{
	Write32BitSwizzler(p, GX_TF_RGBA8);

	WRITE(p, "  float cl1 = xb - (halfxb * 2);\n");
	WRITE(p, "  float cl0 = 1.0f - cl1;\n");

	WRITE(p, "  float4 texSample;\n");
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, "rgba", "texSample");
	WRITE(p, "  color0.b = texSample.a;\n");
	WRITE(p, "  color0.g = texSample.r;\n");
	WRITE(p, "  color1.b = texSample.g;\n");
	WRITE(p, "  color1.g = texSample.b;\n");

	WriteIncrementSampleX(p);

	WriteSampleColor(p, "rgba", "texSample");
	WRITE(p, "  color0.r = texSample.a;\n");
	WRITE(p, "  color0.a = texSample.r;\n");
	WRITE(p, "  color1.r = texSample.g;\n");
	WRITE(p, "  color1.a = texSample.b;\n");

	WRITE(p, "  ocol0 = (cl0 * color0) + (cl1 * color1);\n");

	WRITE(p, "}\n");
}

void WriteC4Encoder(char* p, const char* comp)
{
	WriteSwizzler(p, GX_CTF_R4);
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, comp, "color0.b");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, comp, "color1.b");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, comp, "color0.g");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, comp, "color1.g");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, comp, "color0.r");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, comp, "color1.r");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, comp, "color0.a");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, comp, "color1.a");

	WriteToBitDepth(p, 4, "color0", "color0");
	WriteToBitDepth(p, 4, "color1", "color1");

	WRITE(p, "  ocol0 = (color0 * 16.0f + color1) / 255.0f;\n");
	WRITE(p, "}\n");
}

void WriteC8Encoder(char* p, const char* comp)
{
	WriteSwizzler(p, GX_CTF_R8);

	WriteSampleColor(p, comp, "ocol0.b");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, comp, "ocol0.g");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, comp, "ocol0.r");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, comp, "ocol0.a");

	WRITE(p, "}\n");
}

void WriteCC4Encoder(char* p, const char* comp)
{
	WriteSwizzler(p, GX_CTF_RA4);
	WRITE(p, "  float2 texSample;\n");
	WRITE(p, "  float4 color0;\n");
	WRITE(p, "  float4 color1;\n");

	WriteSampleColor(p, comp, "texSample");
	WRITE(p, "  color0.b = texSample.x;\n");
	WRITE(p, "  color1.b = texSample.y;\n");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, comp, "texSample");
	WRITE(p, "  color0.g = texSample.x;\n");
	WRITE(p, "  color1.g = texSample.y;\n");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, comp, "texSample");
	WRITE(p, "  color0.r = texSample.x;\n");
	WRITE(p, "  color1.r = texSample.y;\n");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, comp, "texSample");
	WRITE(p, "  color0.a = texSample.x;\n");
	WRITE(p, "  color1.a = texSample.y;\n");

	WriteToBitDepth(p, 4, "color0", "color0");
	WriteToBitDepth(p, 4, "color1", "color1");

	WRITE(p, "  ocol0 = (color0 * 16.0f + color1) / 255.0f;\n");
	WRITE(p, "}\n");
}

void WriteCC8Encoder(char* p, const char* comp)
{
	WriteSwizzler(p, GX_CTF_RA8);

	WriteSampleColor(p, comp, "ocol0.bg");
	WriteIncrementSampleX(p);

	WriteSampleColor(p, comp, "ocol0.ra");

	WRITE(p, "}\n");
}

void WriteZ8Encoder(char* p, const char* multiplier)
{
	WriteSwizzler(p, GX_CTF_Z8M);

    WRITE(p, " float depth;\n");

	WriteSampleColor(p, "b", "depth");
    WRITE(p, "ocol0.b = frac(depth * %s);\n", multiplier);
	WriteIncrementSampleX(p);

    WriteSampleColor(p, "b", "depth");
    WRITE(p, "ocol0.g = frac(depth * %s);\n", multiplier);
	WriteIncrementSampleX(p);

    WriteSampleColor(p, "b", "depth");
    WRITE(p, "ocol0.r = frac(depth * %s);\n", multiplier);
	WriteIncrementSampleX(p);

    WriteSampleColor(p, "b", "depth");
    WRITE(p, "ocol0.a = frac(depth * %s);\n", multiplier);

	WRITE(p, "}\n");
}

void WriteZ16Encoder(char* p)
{
    WriteSwizzler(p, GX_TF_Z16);

    WRITE(p, " float depth;\n");

    // byte order is reversed

    WriteSampleColor(p, "b", "depth");
    WRITE(p, "  ocol0.b = frac(depth * 256.0f);\n");
    WRITE(p, "  ocol0.g = depth;\n");

    WriteIncrementSampleX(p);

    WriteSampleColor(p, "b", "depth");
    WRITE(p, "  ocol0.r = frac(depth * 256.0f);\n");
    WRITE(p, "  ocol0.a = depth;\n");    

    WRITE(p, "}\n");
}

void WriteZ16LEncoder(char* p)
{
    WriteSwizzler(p, GX_CTF_Z16L);

    WRITE(p, " float depth;\n");

    // byte order is reversed

    WriteSampleColor(p, "b", "depth");
    WRITE(p, "  ocol0.b = frac(depth * 65536.0f);\n");
    WRITE(p, "  ocol0.g = frac(depth * 256.0f);\n");

    WriteIncrementSampleX(p);

    WriteSampleColor(p, "b", "depth");
    WRITE(p, "  ocol0.r = frac(depth * 65536.0f);\n");
    WRITE(p, "  ocol0.a = frac(depth * 256.0f);\n");    

    WRITE(p, "}\n");
}

void WriteZ24Encoder(char* p)
{
	Write32BitSwizzler(p, GX_TF_Z24X8);

	WRITE(p, "  float cl = xb - (halfxb * 2);\n");

	WRITE(p, "  float depth0;\n");
    WRITE(p, "  float depth1;\n");

	WriteSampleColor(p, "b", "depth0");
    WriteIncrementSampleX(p);
    WriteSampleColor(p, "b", "depth1");

    WRITE(p, "  if(cl > 0.5f) {\n");
    // upper 16
    WRITE(p, "     ocol0.b = frac(depth0 * 256.0f);\n");
    WRITE(p, "     ocol0.g = depth0\n");
    WRITE(p, "     ocol0.r = frac(depth1 * 256.0f);\n");
    WRITE(p, "     ocol0.a = depth1\n");
    WRITE(p, "  } else {\n");
    // lower 8
    WRITE(p, "     ocol0.b = 1.0f;\n");
    WRITE(p, "     ocol0.g = frac(depth0 * 65536.0f)\n");
    WRITE(p, "     ocol0.r = 1.0f);\n");
    WRITE(p, "     ocol0.a = frac(depth0 * 65536.0f)\n");
    WRITE(p, "  }\n"
             "}\n");
}

const char *GenerateEncodingShader(u32 format)
{
	text[sizeof(text) - 1] = 0x7C;  // canary

	char *p = text;

	switch(format)
	{
	case GX_TF_I4:
		WriteI4Encoder(p);
		break;
	case GX_TF_I8:
		WriteI8Encoder(p);
		break;
	case GX_TF_IA4:
		WriteIA4Encoder(p);
		break;
	case GX_TF_IA8:
		WriteIA8Encoder(p);
		break;
	case GX_TF_RGB565:
		WriteRGB565Encoder(p);
		break;
	case GX_TF_RGB5A3:
		WriteRGB5A3Encoder(p);
		break;
	case GX_TF_RGBA8:
		WriteRGBA8Encoder(p);
		break;
	case GX_CTF_R4:
		WriteC4Encoder(p, "r");
		break;
	case GX_CTF_RA4:
		WriteCC4Encoder(p, "ar");
		break;
	case GX_CTF_RA8:
		WriteCC8Encoder(p, "ar");
		break;
	case GX_CTF_A8:
		WriteC8Encoder(p, "a");
		break;
	case GX_CTF_R8:
		WriteC8Encoder(p, "r");
		break;
	case GX_CTF_G8:
		WriteC8Encoder(p, "g");
		break;
	case GX_CTF_B8:
		WriteC8Encoder(p, "b");
		break;
	case GX_CTF_RG8:
		WriteCC8Encoder(p, "rg");
		break;
	case GX_CTF_GB8:
		WriteCC8Encoder(p, "gb");
		break;
	case GX_TF_Z8:
		WriteC8Encoder(p, "b");
		break;
	case GX_TF_Z16:
		WriteZ16Encoder(p);
		break;
	case GX_TF_Z24X8:
		WriteZ24Encoder(p);
		break;
	case GX_CTF_Z4:
		WriteC4Encoder(p, "b");
		break;
	case GX_CTF_Z8M:
		WriteZ8Encoder(p, "256.0f");
		break;
	case GX_CTF_Z8L:
		WriteZ8Encoder(p, "65536.0f" );
		break;
	case GX_CTF_Z16L:
		WriteZ16LEncoder(p);
		break;
	default:
		PanicAlert("Unknown texture copy format: 0x%x\n", format);
		break;		
	}

	if (text[sizeof(text) - 1] != 0x7C)
		PanicAlert("TextureConversionShader generator - buffer too small, canary has been eaten!");

    return text;
}

void SetShaderParameters(float width, float height, float offsetX, float offsetY, float widthStride, float heightStride)
{
	SetPSConstant4f(C_COLORMATRIX, widthStride, heightStride, 0.0f, 0.0f);
	SetPSConstant4f(C_COLORMATRIX + 1, width, (height - 1), offsetX, offsetY);
}

}  // namespace
