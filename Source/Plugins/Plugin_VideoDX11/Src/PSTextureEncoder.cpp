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

#include "PSTextureEncoder.h"

#include "BPMemory.h"
#include "D3DBase.h"
#include "D3DShader.h"
#include "EFBCopy.h"
#include "FramebufferManager.h"
#include "GfxState.h"
#include "HW/Memmap.h"
#include "Render.h"
#include "TextureCache.h"

namespace DX11
{
	
struct EFBEncodeParams
{
	FLOAT Matrix[4*4];
	FLOAT Add[4];
	// Rectangle within EFBTexture containing the EFB (in texels)
	FLOAT TexUL[2]; // Upper left
	FLOAT TexLR[2]; // Lower right
	FLOAT Pos[2]; // Upper left of source (in EFB pixels)
	BOOL DisableAlpha; // If true, alpha will read as 1 from source
};

static const char EFB_ENCODE_PS[] =
"// dolphin-emu EFB encoder pixel shader\n"

// Input

"cbuffer cbParams : register(b0)\n"
"{\n"
	"struct\n" // Should match EFBEncodeParams above
	"{\n"
		"float4x4 Matrix;\n"
		"float4 Add;\n"
		"float2 TexUL;\n"
		"float2 TexLR;\n"
		"float2 Pos;\n"
		"bool DisableAlpha;\n"
	"} Params;\n"
"}\n"

"Texture2D EFBTexture : register(t0);\n"

// Constants

"static const float2 INV_EFB_DIMS = float2(1.0/640.0, 1.0/528.0);\n"

// Utility functions

"uint4 Swap4_32(uint4 v) {\n"
	"return (((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | ((v << 24) & 0xFF000000));\n"
"}\n"

"uint4 UINT4_8888_BE(uint4 a, uint4 b, uint4 c, uint4 d) {\n"
	"return (d << 24) | (c << 16) | (b << 8) | a;\n"
"}\n"

"uint UINT_44444444_BE(uint a, uint b, uint c, uint d, uint e, uint f, uint g, uint h) {\n"
	"return (g << 28) | (h << 24) | (e << 20) | (f << 16) | (c << 12) | (d << 8) | (a << 4) | b;\n"
"}\n"

"uint UINT_1555(uint a, uint b, uint c, uint d) {\n"
	"return (a << 15) | (b << 10) | (c << 5) | d;\n"
"}\n"

"uint UINT_3444(uint a, uint b, uint c, uint d) {\n"
	"return (a << 12) | (b << 8) | (c << 4) | d;\n"
"}\n"

"uint UINT_565(uint a, uint b, uint c) {\n"
	"return (a << 11) | (b << 5) | c;\n"
"}\n"

"uint UINT_1616(uint a, uint b) {\n"
	"return (a << 16) | b;\n"
"}\n"

"uint EncodeRGB5A3(float4 pixel) {\n"
	"if (pixel.a >= 224.0/255.0) {\n"
		// Encode to ARGB1555
		"return UINT_1555(1, pixel.r*31, pixel.g*31, pixel.b*31);\n"
	"} else {\n"
		// Encode to ARGB3444
		"return UINT_3444(pixel.a*7, pixel.r*15, pixel.g*15, pixel.b*15);\n"
	"}\n"
"}\n"

"uint EncodeRGB565(float4 pixel) {\n"
	"return UINT_565(pixel.r*31, pixel.g*63, pixel.b*31);\n"
"}\n"

"float2 CalcTexCoord(float2 coord)\n"
"{\n"
	// Add 0.5,0.5 to sample from the center of the EFB pixel
	"float2 efbCoord = coord + float2(0.5,0.5);\n"
	"return lerp(Params.TexUL, Params.TexLR, efbCoord * INV_EFB_DIMS);\n"
"}\n"

// Interface and classes for different source formats

"float4 Fetch_Color(float2 coord)\n"
"{\n"
	"float2 texCoord = CalcTexCoord(coord);\n"
	"return EFBTexture.Load(int3(texCoord, 0));\n"
"}\n"

"float4 Fetch_Depth(float2 coord)\n"
"{\n"
	"float2 texCoord = CalcTexCoord(coord);\n"
	"uint depth24 = 0xFFFFFF * EFBTexture.Load(int3(texCoord, 0)).r;\n"
	"uint4 bytes = uint4(\n"
		"(depth24 >> 16) & 0xFF,\n" // r
		"(depth24 >> 8) & 0xFF,\n"  // g
		"depth24 & 0xFF,\n"         // b
		"255);\n"                   // a
	"return bytes / 255.0;\n"
"}\n"

"#ifndef IMP_FETCH\n"
"#error No Fetch specified\n"
"#endif\n"

// Interface and classes for different scale/filter settings (on or off)

"float4 ScaledFetch_0(float2 coord)\n"
"{\n"
	"return IMP_FETCH(Params.Pos + coord);\n"
"}\n"

"float4 ScaledFetch_1(float2 coord)\n"
"{\n"
	"float2 ul = Params.Pos + 2*coord;\n"
	"float4 sample0 = IMP_FETCH(ul+float2(0,0));\n"
	"float4 sample1 = IMP_FETCH(ul+float2(1,0));\n"
	"float4 sample2 = IMP_FETCH(ul+float2(0,1));\n"
	"float4 sample3 = IMP_FETCH(ul+float2(1,1));\n"
	// Average all four samples together
	// FIXME: Is this correct?
	"return 0.25 * (sample0+sample1+sample2+sample3);\n"
"}\n"

"#ifndef IMP_SCALEDFETCH\n"
"#error No ScaledFetch specified\n"
"#endif\n"

// Main EFB-sampling function: performs all steps of fetching pixels, scaling,
// and applying transforms

"float SampleEFB_R(float2 coord)\n"
"{\n"
	"float4 sample = IMP_SCALEDFETCH(coord);\n"
	"if (Params.DisableAlpha)\n"
		"sample.a = 1;\n"
	"return (mul(sample, Params.Matrix) + Params.Add).r;\n"
"}\n"

"float2 SampleEFB_RG(float2 coord)\n"
"{\n"
	"float4 sample = IMP_SCALEDFETCH(coord);\n"
	"if (Params.DisableAlpha)\n"
		"sample.a = 1;\n"
	"return (mul(sample, Params.Matrix) + Params.Add).rg;\n"
"}\n"

"float4 SampleEFB_RGBA(float2 coord)\n"
"{\n"
	"float4 sample = IMP_SCALEDFETCH(coord);\n"
	"if (Params.DisableAlpha)\n"
		"sample.a = 1;\n"
	"return mul(sample, Params.Matrix) + Params.Add;\n"
"}\n"

// Interfaces and classes for different destination formats

"uint4 Generate_R4(float2 cacheCoord)\n"
"{\n"
	"float2 blockCoord = floor(cacheCoord / float2(2,1));\n"

	"float2 blockUL = blockCoord * float2(8,8);\n"
	"float2 subBlockUL = blockUL + float2(0, 4*(cacheCoord.x%2));\n"

	"float sample[32];\n"
	"for (uint y = 0; y < 4; ++y) {\n"
		"for (uint x = 0; x < 8; ++x) {\n"
			"sample[y*8+x] = SampleEFB_R(subBlockUL+float2(x,y));\n"
		"}\n"
	"}\n"
		
	"uint dw[4];\n"
	"for (uint i = 0; i < 4; ++i) {\n"
		"dw[i] = UINT_44444444_BE(\n"
			"15*sample[8*i+0],\n"
			"15*sample[8*i+1],\n"
			"15*sample[8*i+2],\n"
			"15*sample[8*i+3],\n"
			"15*sample[8*i+4],\n"
			"15*sample[8*i+5],\n"
			"15*sample[8*i+6],\n"
			"15*sample[8*i+7]\n"
			");\n"
	"}\n"

	"return uint4(dw[0], dw[1], dw[2], dw[3]);\n"
"}\n"

"uint4 Generate_R8(float2 cacheCoord)\n"
"{\n"
	"float2 blockCoord = floor(cacheCoord / float2(2,1));\n"

	"float2 blockUL = blockCoord * float2(8,4);\n"
	"float2 subBlockUL = blockUL + float2(0, 2*(cacheCoord.x%2));\n"

	"float sample0 = SampleEFB_R(subBlockUL+float2(0,0));\n"
	"float sample1 = SampleEFB_R(subBlockUL+float2(1,0));\n"
	"float sample2 = SampleEFB_R(subBlockUL+float2(2,0));\n"
	"float sample3 = SampleEFB_R(subBlockUL+float2(3,0));\n"
	"float sample4 = SampleEFB_R(subBlockUL+float2(4,0));\n"
	"float sample5 = SampleEFB_R(subBlockUL+float2(5,0));\n"
	"float sample6 = SampleEFB_R(subBlockUL+float2(6,0));\n"
	"float sample7 = SampleEFB_R(subBlockUL+float2(7,0));\n"
	"float sample8 = SampleEFB_R(subBlockUL+float2(0,1));\n"
	"float sample9 = SampleEFB_R(subBlockUL+float2(1,1));\n"
	"float sampleA = SampleEFB_R(subBlockUL+float2(2,1));\n"
	"float sampleB = SampleEFB_R(subBlockUL+float2(3,1));\n"
	"float sampleC = SampleEFB_R(subBlockUL+float2(4,1));\n"
	"float sampleD = SampleEFB_R(subBlockUL+float2(5,1));\n"
	"float sampleE = SampleEFB_R(subBlockUL+float2(6,1));\n"
	"float sampleF = SampleEFB_R(subBlockUL+float2(7,1));\n"

	"uint4 dw4 = UINT4_8888_BE(\n"
		"255*float4(sample0, sample4, sample8, sampleC),\n"
		"255*float4(sample1, sample5, sample9, sampleD),\n"
		"255*float4(sample2, sample6, sampleA, sampleE),\n"
		"255*float4(sample3, sample7, sampleB, sampleF)\n"
		");\n"
	
	"return dw4;\n"
"}\n"

// FIXME: Untested
"uint4 Generate_RG4(float2 cacheCoord)\n"
"{\n"
	"float2 blockCoord = floor(cacheCoord / float2(2,1));\n"

	"float2 blockUL = blockCoord * float2(8,4);\n"
	"float2 subBlockUL = blockUL + float2(0, 2*(cacheCoord.x%2));\n"
	
	"float2 sample0 = SampleEFB_RG(subBlockUL+float2(0,0));\n"
	"float2 sample1 = SampleEFB_RG(subBlockUL+float2(1,0));\n"
	"float2 sample2 = SampleEFB_RG(subBlockUL+float2(2,0));\n"
	"float2 sample3 = SampleEFB_RG(subBlockUL+float2(3,0));\n"
	"float2 sample4 = SampleEFB_RG(subBlockUL+float2(4,0));\n"
	"float2 sample5 = SampleEFB_RG(subBlockUL+float2(5,0));\n"
	"float2 sample6 = SampleEFB_RG(subBlockUL+float2(6,0));\n"
	"float2 sample7 = SampleEFB_RG(subBlockUL+float2(7,0));\n"
	"float2 sample8 = SampleEFB_RG(subBlockUL+float2(0,1));\n"
	"float2 sample9 = SampleEFB_RG(subBlockUL+float2(1,1));\n"
	"float2 sampleA = SampleEFB_RG(subBlockUL+float2(2,1));\n"
	"float2 sampleB = SampleEFB_RG(subBlockUL+float2(3,1));\n"
	"float2 sampleC = SampleEFB_RG(subBlockUL+float2(4,1));\n"
	"float2 sampleD = SampleEFB_RG(subBlockUL+float2(5,1));\n"
	"float2 sampleE = SampleEFB_RG(subBlockUL+float2(6,1));\n"
	"float2 sampleF = SampleEFB_RG(subBlockUL+float2(7,1));\n"

	"uint dw0 = UINT_44444444_BE(\n"
		"15*sample0.r, 15*sample0.g,\n"
		"15*sample1.r, 15*sample1.g,\n"
		"15*sample2.r, 15*sample2.g,\n"
		"15*sample3.r, 15*sample3.g\n"
		");\n"
	"uint dw1 = UINT_44444444_BE(\n"
		"15*sample4.r, 15*sample4.g,\n"
		"15*sample5.r, 15*sample5.g,\n"
		"15*sample6.r, 15*sample6.g,\n"
		"15*sample7.r, 15*sample7.g\n"
		");\n"
	"uint dw2 = UINT_44444444_BE(\n"
		"15*sample8.r, 15*sample8.g,\n"
		"15*sample9.r, 15*sample9.g,\n"
		"15*sampleA.r, 15*sampleA.g,\n"
		"15*sampleB.r, 15*sampleB.g\n"
		");\n"
	"uint dw3 = UINT_44444444_BE(\n"
		"15*sampleC.r, 15*sampleC.g,\n"
		"15*sampleD.r, 15*sampleD.g,\n"
		"15*sampleE.r, 15*sampleE.g,\n"
		"15*sampleF.r, 15*sampleF.g\n"
		");\n"
	
	"return uint4(dw0, dw1, dw2, dw3);\n"
"}\n"

"uint4 Generate_RG8(float2 cacheCoord)\n"
"{\n"
	"float2 blockCoord = floor(cacheCoord / float2(2,1));\n"

	"float2 blockUL = blockCoord * float2(4,4);\n"
	"float2 subBlockUL = blockUL + float2(0, 2*(cacheCoord.x%2));\n"

	"float2 sample0 = SampleEFB_RG(subBlockUL+float2(0,0));\n"
	"float2 sample1 = SampleEFB_RG(subBlockUL+float2(1,0));\n"
	"float2 sample2 = SampleEFB_RG(subBlockUL+float2(2,0));\n"
	"float2 sample3 = SampleEFB_RG(subBlockUL+float2(3,0));\n"
	"float2 sample4 = SampleEFB_RG(subBlockUL+float2(0,1));\n"
	"float2 sample5 = SampleEFB_RG(subBlockUL+float2(1,1));\n"
	"float2 sample6 = SampleEFB_RG(subBlockUL+float2(2,1));\n"
	"float2 sample7 = SampleEFB_RG(subBlockUL+float2(3,1));\n"

	"uint4 dw4 = UINT4_8888_BE(\n"
		"255*float4(sample0.r, sample2.r, sample4.r, sample6.r),\n"
		"255*float4(sample0.g, sample2.g, sample4.g, sample6.g),\n"
		"255*float4(sample1.r, sample3.r, sample5.r, sample7.r),\n"
		"255*float4(sample1.g, sample3.g, sample5.g, sample7.g)\n"
		");\n"
	
	"return dw4;\n"
"}\n"

"uint4 Generate_RGB565(float2 cacheCoord)\n"
"{\n"
	"float2 blockCoord = floor(cacheCoord / float2(2,1));\n"

	"float2 blockUL = blockCoord * float2(4,4);\n"
	"float2 subBlockUL = blockUL + float2(0, 2*(cacheCoord.x%2));\n"

	"float4 sample0 = SampleEFB_RGBA(subBlockUL+float2(0,0));\n"
	"float4 sample1 = SampleEFB_RGBA(subBlockUL+float2(1,0));\n"
	"float4 sample2 = SampleEFB_RGBA(subBlockUL+float2(2,0));\n"
	"float4 sample3 = SampleEFB_RGBA(subBlockUL+float2(3,0));\n"
	"float4 sample4 = SampleEFB_RGBA(subBlockUL+float2(0,1));\n"
	"float4 sample5 = SampleEFB_RGBA(subBlockUL+float2(1,1));\n"
	"float4 sample6 = SampleEFB_RGBA(subBlockUL+float2(2,1));\n"
	"float4 sample7 = SampleEFB_RGBA(subBlockUL+float2(3,1));\n"
		
	"uint dw0 = UINT_1616(EncodeRGB565(sample0), EncodeRGB565(sample1));\n"
	"uint dw1 = UINT_1616(EncodeRGB565(sample2), EncodeRGB565(sample3));\n"
	"uint dw2 = UINT_1616(EncodeRGB565(sample4), EncodeRGB565(sample5));\n"
	"uint dw3 = UINT_1616(EncodeRGB565(sample6), EncodeRGB565(sample7));\n"

	"return Swap4_32(uint4(dw0, dw1, dw2, dw3));\n"
"}\n"

"uint4 Generate_RGB5A3(float2 cacheCoord)\n"
"{\n"
	"float2 blockCoord = floor(cacheCoord / float2(2,1));\n"

	"float2 blockUL = blockCoord * float2(4,4);\n"
	"float2 subBlockUL = blockUL + float2(0, 2*(cacheCoord.x%2));\n"

	"float4 sample0 = SampleEFB_RGBA(subBlockUL+float2(0,0));\n"
	"float4 sample1 = SampleEFB_RGBA(subBlockUL+float2(1,0));\n"
	"float4 sample2 = SampleEFB_RGBA(subBlockUL+float2(2,0));\n"
	"float4 sample3 = SampleEFB_RGBA(subBlockUL+float2(3,0));\n"
	"float4 sample4 = SampleEFB_RGBA(subBlockUL+float2(0,1));\n"
	"float4 sample5 = SampleEFB_RGBA(subBlockUL+float2(1,1));\n"
	"float4 sample6 = SampleEFB_RGBA(subBlockUL+float2(2,1));\n"
	"float4 sample7 = SampleEFB_RGBA(subBlockUL+float2(3,1));\n"
		
	"uint dw0 = UINT_1616(EncodeRGB5A3(sample0), EncodeRGB5A3(sample1));\n"
	"uint dw1 = UINT_1616(EncodeRGB5A3(sample2), EncodeRGB5A3(sample3));\n"
	"uint dw2 = UINT_1616(EncodeRGB5A3(sample4), EncodeRGB5A3(sample5));\n"
	"uint dw3 = UINT_1616(EncodeRGB5A3(sample6), EncodeRGB5A3(sample7));\n"
	
	"return Swap4_32(uint4(dw0, dw1, dw2, dw3));\n"
"}\n"

"uint4 Generate_RGBA8(float2 cacheCoord)\n"
"{\n"
	"float2 blockCoord = floor(cacheCoord / float2(4,1));\n"

	"float2 blockUL = blockCoord * float2(4,4);\n"
	"float2 subBlockUL = blockUL + float2(0, 2*(cacheCoord.x%2));\n"

	"float4 sample0 = SampleEFB_RGBA(subBlockUL+float2(0,0));\n"
	"float4 sample1 = SampleEFB_RGBA(subBlockUL+float2(1,0));\n"
	"float4 sample2 = SampleEFB_RGBA(subBlockUL+float2(2,0));\n"
	"float4 sample3 = SampleEFB_RGBA(subBlockUL+float2(3,0));\n"
	"float4 sample4 = SampleEFB_RGBA(subBlockUL+float2(0,1));\n"
	"float4 sample5 = SampleEFB_RGBA(subBlockUL+float2(1,1));\n"
	"float4 sample6 = SampleEFB_RGBA(subBlockUL+float2(2,1));\n"
	"float4 sample7 = SampleEFB_RGBA(subBlockUL+float2(3,1));\n"

	"uint4 dw4;\n"
	"if (cacheCoord.x % 4 < 2)\n"
	"{\n"
		// First cache line gets AR
		"dw4 = UINT4_8888_BE(\n"
			"255*float4(sample0.a, sample2.a, sample4.a, sample6.a),\n"
			"255*float4(sample0.r, sample2.r, sample4.r, sample6.r),\n"
			"255*float4(sample1.a, sample3.a, sample5.a, sample7.a),\n"
			"255*float4(sample1.r, sample3.r, sample5.r, sample7.r)\n"
			");\n"
	"}\n"
	"else\n"
	"{\n"
		// Second cache line gets GB
		"dw4 = UINT4_8888_BE(\n"
			"255*float4(sample0.g, sample2.g, sample4.g, sample6.g),\n"
			"255*float4(sample0.b, sample2.b, sample4.b, sample6.b),\n"
			"255*float4(sample1.g, sample3.g, sample5.g, sample7.g),\n"
			"255*float4(sample1.b, sample3.b, sample5.b, sample7.b)\n"
			");\n"
	"}\n"
	
	"return dw4;\n"
"}\n"

"#ifndef IMP_GENERATOR\n"
"#error No generator specified\n"
"#endif\n"

"void main(out uint4 ocol0 : SV_Target, in float4 Pos : SV_Position)\n"
"{\n"
	"float2 cacheCoord = floor(Pos.xy);\n"
	"ocol0 = IMP_GENERATOR(cacheCoord);\n"
"}\n"
;

PSTextureEncoder::PSTextureEncoder()
	: m_ready(false), m_out(NULL), m_outRTV(NULL), m_outStage(NULL), m_encodeParams(NULL)
{
	HRESULT hr;

	// Create output texture RGBA format

	// This format allows us to generate one cache line in two pixels.
	D3D11_TEXTURE2D_DESC t2dd = CD3D11_TEXTURE2D_DESC(
		DXGI_FORMAT_R32G32B32A32_UINT,
		EFB_WIDTH, EFB_HEIGHT/4, 1, 1, D3D11_BIND_RENDER_TARGET);
	hr = D3D::device->CreateTexture2D(&t2dd, NULL, &m_out);
	CHECK(SUCCEEDED(hr), "create efb encode output texture");
	D3D::SetDebugObjectName(m_out, "efb encoder output texture");

	// Create output render target view

	D3D11_RENDER_TARGET_VIEW_DESC rtvd = CD3D11_RENDER_TARGET_VIEW_DESC(m_out,
		D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R32G32B32A32_UINT);
	hr = D3D::device->CreateRenderTargetView(m_out, &rtvd, &m_outRTV);
	CHECK(SUCCEEDED(hr), "create efb encode output render target view");
	D3D::SetDebugObjectName(m_outRTV, "efb encoder output rtv");

	// Create output staging buffer
	
	t2dd.Usage = D3D11_USAGE_STAGING;
	t2dd.BindFlags = 0;
	t2dd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	hr = D3D::device->CreateTexture2D(&t2dd, NULL, &m_outStage);
	CHECK(SUCCEEDED(hr), "create efb encode output staging buffer");
	D3D::SetDebugObjectName(m_outStage, "efb encoder output staging buffer");

	// Create constant buffer for uploading data to shaders

	D3D11_BUFFER_DESC bd = CD3D11_BUFFER_DESC((sizeof(EFBEncodeParams) + 15) & ~15,
		D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	hr = D3D::device->CreateBuffer(&bd, NULL, &m_encodeParams);
	CHECK(SUCCEEDED(hr), "create efb encode params buffer");
	D3D::SetDebugObjectName(m_encodeParams, "efb encoder params buffer");

	// Create pixel shader

	if (!InitStaticMode())
		return;

	m_ready = true;
}

PSTextureEncoder::~PSTextureEncoder()
{
	for (ComboMap::iterator it = m_staticShaders.begin();
		it != m_staticShaders.end(); ++it)
	{
		SAFE_RELEASE(it->second);
	}

	SAFE_RELEASE(m_encodeParams);
	SAFE_RELEASE(m_outStage);
	SAFE_RELEASE(m_outRTV);
	SAFE_RELEASE(m_out);
}

u32 PSTextureEncoder::Encode(u8* dst, unsigned int dstFormat, D3DTexture2D* srcTex,
	unsigned int srcFormat, const EFBRectangle& srcRect, bool isIntensity,
	bool scaleByHalf)
{
	if (!m_ready) // Make sure we initialized OK
		return 0;

	// Clamp srcRect to 640x528. BPS: The Strike tries to encode an 800x600
	// texture, which is invalid.
	EFBRectangle correctSrc = srcRect;
	correctSrc.ClampUL(0, 0, EFB_WIDTH, EFB_HEIGHT);

	// Validate source rect size
	if (correctSrc.GetWidth() <= 0 || correctSrc.GetHeight() <= 0)
		return 0;

	HRESULT hr;

	unsigned int blockW = EFB_COPY_BLOCK_WIDTHS[dstFormat];
	unsigned int blockH = EFB_COPY_BLOCK_HEIGHTS[dstFormat];

	// Round up source dims to multiple of block size
	unsigned int actualWidth = correctSrc.GetWidth() / (scaleByHalf ? 2 : 1);
	actualWidth = (actualWidth + blockW-1) & ~(blockW-1);
	unsigned int actualHeight = correctSrc.GetHeight() / (scaleByHalf ? 2 : 1);
	actualHeight = (actualHeight + blockH-1) & ~(blockH-1);

	unsigned int numBlocksX = actualWidth/blockW;
	unsigned int numBlocksY = actualHeight/blockH;

	unsigned int cacheLinesPerRow;
	if (dstFormat == EFB_COPY_RGBA8) // RGBA takes two cache lines per block; all others take one
		cacheLinesPerRow = numBlocksX*2;
	else
		cacheLinesPerRow = numBlocksX;
	_assert_msg_(VIDEO, cacheLinesPerRow*32 <= EFB_COPY_MAX_BYTES_PER_ROW, "cache lines per row sanity check");

	unsigned int totalCacheLines = cacheLinesPerRow * numBlocksY;
	_assert_msg_(VIDEO, totalCacheLines*32 <= EFB_COPY_MAX_BYTES, "total encode size sanity check");

	INFO_LOG(VIDEO, "Encoding dstFormat %s, intensity %d, scale %d",
		EFB_COPY_DST_FORMAT_NAMES[dstFormat], isIntensity ? 1 : 0, scaleByHalf ? 1 : 0);

	size_t encodeSize = 0;
	
	// Reset API

	g_renderer->ResetAPIState();
	D3D::stateman->Apply();

	// Set up all the state for EFB encoding
	
	if (SetStaticShader(dstFormat, srcFormat, isIntensity, scaleByHalf))
	{
		D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, FLOAT(cacheLinesPerRow*2), FLOAT(numBlocksY));
		D3D::context->RSSetViewports(1, &vp);
	
		EFBRectangle fullSrcRect;
		fullSrcRect.left = 0;
		fullSrcRect.top = 0;
		fullSrcRect.right = EFB_WIDTH;
		fullSrcRect.bottom = EFB_HEIGHT;
		TargetRectangle targetRect = g_renderer->ConvertEFBRectangle(fullSrcRect);

		static const float YUVA_MATRIX[4*4] = {
			0.257f, 0.504f, 0.098f, 0.f,
			-0.148f, -0.291f, 0.439f, 0.f,
			0.439f, -0.368f, -0.071f, 0.f,
			0.f, 0.f, 0.f, 1.f
		};
		static const float YUV_ADD[4] = { 16.f/255.f, 128.f/255.f, 128.f/255.f };
		
		D3D11_MAPPED_SUBRESOURCE map;
		hr = D3D::context->Map(m_encodeParams, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		if (SUCCEEDED(hr))
		{
			EFBEncodeParams* params = (EFBEncodeParams*)map.pData;
			
			// Choose a pre-color matrix: Either RGBA or YUVA
			if (isIntensity)
			{
				// Combine YUVA matrix with color matrix
				Matrix44 colorMat;
				Matrix44::Set(colorMat, m_useThisMatrix);
				Matrix44 yuvaMat;
				Matrix44::Set(yuvaMat, YUVA_MATRIX);
				Matrix44 combinedMat;
				Matrix44::Multiply(colorMat, yuvaMat, combinedMat);
				
				memcpy(params->Matrix, combinedMat.data, 4*4*sizeof(float));

				for (int i = 0; i < 3; ++i)
					params->Add[i] = m_useThisAdd[i] + YUV_ADD[i];
				params->Add[3] = m_useThisAdd[3];
			}
			else
			{
				memcpy(params->Matrix, m_useThisMatrix, 4*4*sizeof(float));
				memcpy(params->Add, m_useThisAdd, 4*sizeof(float));
			}

			params->TexUL[0] = FLOAT(targetRect.left);
			params->TexUL[1] = FLOAT(targetRect.top);
			params->TexLR[0] = FLOAT(targetRect.right);
			params->TexLR[1] = FLOAT(targetRect.bottom);
			params->Pos[0] = FLOAT(correctSrc.left);
			params->Pos[1] = FLOAT(correctSrc.top);

			params->DisableAlpha = (srcFormat != PIXELFMT_RGBA6_Z24) ? TRUE : FALSE;

			D3D::context->Unmap(m_encodeParams, 0);
		}
		else
			ERROR_LOG(VIDEO, "Failed to map encode params buffer");
	
		D3D::context->OMSetRenderTargets(1, &m_outRTV, NULL);

		D3D::context->PSSetConstantBuffers(0, 1, &m_encodeParams);
		D3D::context->PSSetShaderResources(0, 1, &srcTex->GetSRV());

		// Encode!

		D3D::drawEncoderQuad(m_useThisPS, NULL, 0);

		// Copy to staging buffer
		
		D3D::context->OMSetRenderTargets(0, NULL, NULL);

		D3D11_BOX srcBox = CD3D11_BOX(0, 0, 0, cacheLinesPerRow*2, numBlocksY, 1);
		D3D::context->CopySubresourceRegion(m_outStage, 0, 0, 0, 0, m_out, 0, &srcBox);

		// Clean up state
	
		IUnknown* nullDummy = NULL;
		D3D::context->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&nullDummy);
		D3D::context->PSSetConstantBuffers(0, 1, (ID3D11Buffer**)&nullDummy);

		// Transfer staging buffer to GameCube/Wii RAM

		hr = D3D::context->Map(m_outStage, 0, D3D11_MAP_READ, 0, &map);
		CHECK(SUCCEEDED(hr), "map staging buffer");

		u8* src = (u8*)map.pData;
		for (unsigned int y = 0; y < numBlocksY; ++y)
		{
			memcpy(dst, src, cacheLinesPerRow*32);
			dst += bpmem.copyMipMapStrideChannels*32;
			src += map.RowPitch;
		}

		D3D::context->Unmap(m_outStage, 0);

		encodeSize = bpmem.copyMipMapStrideChannels*32 * numBlocksY;
	}

	// Restore API

	g_renderer->RestoreAPIState();
	D3D::context->OMSetRenderTargets(1,
		&FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());

	return encodeSize;
}

bool PSTextureEncoder::InitStaticMode()
{
	// Nothing to really do.
	return true;
}

// TODO: Move these to a common place
static const float RGBA_MATRIX[4*4] = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1
};

static const float RGB0_MATRIX[4*4] = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 0
};

// FIXME: These don't need to be full 4x4 matrices. But the shader needs it.
static const float PACK_RA_TO_RG_MATRIX[4*4] = {
	1, 0, 0, 0,
	0, 0, 0, 1,
	0, 0, 0, 0,
	0, 0, 0, 0
};

static const float PACK_A_TO_R_MATRIX[4*4] = {
	0, 0, 0, 1,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0
};

static const float PACK_R_TO_R_MATRIX[4*4] = {
	1, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0
};

static const float PACK_G_TO_R_MATRIX[4*4] = {
	0, 1, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0
};

static const float PACK_B_TO_R_MATRIX[4*4] = {
	0, 0, 1, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0
};

static const float PACK_GR_TO_RG_MATRIX[4*4] = {
	0, 1, 0, 0,
	1, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0
};

static const float PACK_BG_TO_RG_MATRIX[4*4] = {
	0, 0, 1, 0,
	0, 1, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0
};

static const float ZERO_ADD[4] = { 0, 0, 0, 0 };
static const float A1_ADD[4] = { 0, 0, 0, 1 };

bool PSTextureEncoder::SetStaticShader(unsigned int dstFormat, unsigned int srcFormat,
	bool isIntensity, bool scaleByHalf)
{
	GeneratorType gen;
	const char* genFuncName;
	const float* matrix;
	const float* add;
	switch (dstFormat)
	{
	case EFB_COPY_R4:
		gen = Generator_R4;
		genFuncName = "Generate_R4";
		matrix = PACK_R_TO_R_MATRIX;
		add = ZERO_ADD;
		break;
	case EFB_COPY_R8_1:
	case EFB_COPY_R8:
		gen = Generator_R8;
		genFuncName = "Generate_R8";
		matrix = PACK_R_TO_R_MATRIX;
		add = ZERO_ADD;
		break;
	case EFB_COPY_RA4:
		gen = Generator_RG4;
		genFuncName = "Generate_RG4";
		matrix = PACK_RA_TO_RG_MATRIX;
		add = ZERO_ADD;
		break;
	case EFB_COPY_RA8:
		gen = Generator_RG8;
		genFuncName = "Generate_RG8";
		matrix = PACK_RA_TO_RG_MATRIX;
		add = ZERO_ADD;
		break;
	case EFB_COPY_RGB565:
		gen = Generator_RGB565;
		genFuncName = "Generate_RGB565";
		matrix = RGB0_MATRIX;
		add = A1_ADD;
		break;
	case EFB_COPY_RGB5A3:
		gen = Generator_RGB5A3;
		genFuncName = "Generate_RGB5A3";
		matrix = RGBA_MATRIX;
		add = ZERO_ADD;
		break;
	case EFB_COPY_RGBA8:
		gen = Generator_RGBA8;
		genFuncName = "Generate_RGBA8";
		matrix = RGBA_MATRIX;
		add = ZERO_ADD;
		break;
	case EFB_COPY_A8:
		gen = Generator_R8;
		genFuncName = "Generate_R8";
		matrix = PACK_A_TO_R_MATRIX;
		add = ZERO_ADD;
		break;
	case EFB_COPY_G8:
		gen = Generator_R8;
		genFuncName = "Generate_R8";
		matrix = PACK_G_TO_R_MATRIX;
		add = ZERO_ADD;
		break;
	case EFB_COPY_B8:
		gen = Generator_R8;
		genFuncName = "Generate_R8";
		matrix = PACK_B_TO_R_MATRIX;
		add = ZERO_ADD;
		break;
	case EFB_COPY_RG8:
		gen = Generator_RG8;
		genFuncName = "Generate_RG8";
		matrix = PACK_GR_TO_RG_MATRIX;
		add = ZERO_ADD;
		break;
	case EFB_COPY_GB8:
		gen = Generator_RG8;
		genFuncName = "Generate_RG8";
		matrix = PACK_BG_TO_RG_MATRIX;
		add = ZERO_ADD;
		break;
	default:
		WARN_LOG(VIDEO, "No generator available for dst format 0x%X; aborting", dstFormat);
		return false;
	}

	ComboKey key = MakeComboKey(gen, srcFormat == PIXELFMT_Z24, scaleByHalf);

	ComboMap::iterator it = m_staticShaders.find(key);
	if (it == m_staticShaders.end())
	{
		const char* fetchFuncName = (srcFormat == PIXELFMT_Z24) ? "Fetch_Depth" : "Fetch_Color";
		const char* scaledFetchFuncName = scaleByHalf ? "ScaledFetch_1" : "ScaledFetch_0";

		INFO_LOG(VIDEO, "Compiling efb encoding shader for dstFormat 0x%X, srcFormat %d, isIntensity %d, scaleByHalf %d",
			dstFormat, srcFormat, isIntensity ? 1 : 0, scaleByHalf ? 1 : 0);

		// Shader permutation not found, so compile it
		D3DBlob* bytecode = NULL;
		D3D_SHADER_MACRO macros[] = {
			{ "IMP_FETCH", fetchFuncName },
			{ "IMP_SCALEDFETCH", scaledFetchFuncName },
			{ "IMP_GENERATOR", genFuncName },
			{ NULL, NULL }
		};
		if (!D3D::CompilePixelShader(EFB_ENCODE_PS, sizeof(EFB_ENCODE_PS), &bytecode, macros))
		{
			WARN_LOG(VIDEO, "EFB encoder shader for dstFormat 0x%X, srcFormat %d scaleByHalf %d failed to compile",
				dstFormat, srcFormat, isIntensity ? 1 : 0, scaleByHalf ? 1 : 0);
			// Add dummy shader to map to prevent trying to compile over and
			// over again
			m_staticShaders[key] = NULL;
			return false;
		}

		ID3D11PixelShader* newShader;
		HRESULT hr = D3D::device->CreatePixelShader(bytecode->Data(), bytecode->Size(), NULL, &newShader);
		CHECK(SUCCEEDED(hr), "create efb encoder pixel shader");

		it = m_staticShaders.insert(std::make_pair(key, newShader)).first;
		bytecode->Release();
	}

	if (it != m_staticShaders.end())
	{
		if (it->second)
		{
			m_useThisPS = it->second;
			m_useThisMatrix = matrix;
			m_useThisAdd = add;
			return true;
		}
		else
			return false;
	}
	else
		return false;
}

}
