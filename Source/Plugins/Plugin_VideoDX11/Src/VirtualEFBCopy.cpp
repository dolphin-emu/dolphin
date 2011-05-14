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

#include "VirtualEFBCopy.h"

#include "FramebufferManager.h"
#include "D3DBase.h"
#include "D3DTexture.h"
#include "D3DShader.h"
#include "EFBCopy.h"
#include "Render.h"
#include "TextureCache.h"
#include "TextureDecoder.h"
#include "VertexShaderCache.h"
#include "GfxState.h"
#include "HW/Memmap.h"

namespace DX11
{

struct VirtCopyShaderParams
{
	FLOAT Matrix[4][4];
	FLOAT Add[4];
	FLOAT Pos[2]; // Top-left of source in texels
	BOOL DisableAlpha; // If true, alpha will read as 1 from the source
};

union VirtCopyShaderParams_Padded
{
	VirtCopyShaderParams params;
	// Constant buffers must be a multiple of 16 bytes in size.
	u8 pad[(sizeof(VirtCopyShaderParams) + 15) & ~15];
};

static const char VIRTUAL_EFB_COPY_PS[] =
"// dolphin-emu DX11 virtual efb copy pixel shader\n"

"cbuffer cbParams : register(b0)\n"
"{\n"
	"struct\n" // Should match struct FakeEncodeParams above
	"{\n"
		"float4x4 Matrix;\n"
		"float4 Add;\n"
		"float2 Pos;\n"
		"bool DisableAlpha;\n"
	"} Params;\n"
"}\n"

"Texture2D<float4> EFBTexture : register(t0);\n"

// DEPTH should be 1 on, 0 off
"#ifndef DEPTH\n"
"#error DEPTH not defined.\n"
"#endif\n"

// SCALE should be 1 on, 0 off
"#ifndef SCALE\n"
"#error SCALE not defined.\n"
"#endif\n"

"#if DEPTH\n"
// Fetch from depth buffer by translating X8 Z24 into A8 R8 G8 B8
"float4 Fetch(float2 pos)\n"
"{\n"
	"float fDepth = EFBTexture.Load(int3(pos, 0)).r;\n"
	"uint depth24 = 0xFFFFFF * fDepth;\n"
	"uint4 bytes = uint4(\n"
		"(depth24 >> 16) & 0xFF,\n" // r
		"(depth24 >> 8) & 0xFF,\n"  // g
		"depth24 & 0xFF,\n"         // b
		"255);\n"                   // a

	"return bytes / 255.0;\n"
"}\n"
"#else\n"
// Fetch from color buffer
"float4 Fetch(float2 pos)\n"
"{\n"
	"return EFBTexture.Load(int3(pos, 0));\n"
"}\n"
"#endif\n"

"#if SCALE\n"
// Scaling is on
"float4 ScaledFetch(float2 pos)\n"
"{\n"
	"float2 tl = floor(pos);\n"

	// FIXME: Is box filter applied to depth fetches? It is here.
	"float4 pixel0 = Fetch(Params.Pos + 2*tl + float2(0,0));\n"
	"float4 pixel1 = Fetch(Params.Pos + 2*tl + float2(1,0));\n"
	"float4 pixel2 = Fetch(Params.Pos + 2*tl + float2(0,1));\n"
	"float4 pixel3 = Fetch(Params.Pos + 2*tl + float2(1,1));\n"

	"return 0.25 * (pixel0 + pixel1 + pixel2 + pixel3);\n"
"}\n"
"#else\n"
// Scaling is off
"float4 ScaledFetch(float2 pos)\n"
"{\n"
	"return Fetch(Params.Pos + pos);\n"
"}\n"
"#endif\n"

"void main(out float4 ocol0 : SV_Target, in float4 pos : SV_Position)\n"
"{\n"
	"float4 pixel = ScaledFetch(pos.xy);\n"
	"if (Params.DisableAlpha)\n"
		"pixel.a = 1;\n"
	"ocol0 = mul(pixel, Params.Matrix) + Params.Add;\n"
"}\n"
;

SharedPtr<ID3D11PixelShader> VirtualCopyShaderManager::GetShader(bool scale, bool depth)
{
	// There are 4 different combinations of scale and depth.
	int key = (depth ? (1<<1) : 0) | (scale ? 1 : 0);

	if (!m_shaderParams)
	{
		D3D11_BUFFER_DESC bd = CD3D11_BUFFER_DESC(sizeof(VirtCopyShaderParams_Padded),
			D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		m_shaderParams = CreateBufferShared(&bd, NULL);
		if (!m_shaderParams)
			ERROR_LOG(VIDEO, "Failed to create fake encode params buffer");
	}

	if (!m_shaders[key])
	{
		D3D_SHADER_MACRO macros[] = {
			{ "DEPTH", depth ? "1" : "0" },
			{ "SCALE", scale ? "1" : "0" },
			{ NULL, NULL }
		};

		m_shaders[key] = D3D::CompileAndCreatePixelShader(
			VIRTUAL_EFB_COPY_PS, sizeof(VIRTUAL_EFB_COPY_PS), macros);
		if (!m_shaders[key])
			ERROR_LOG(VIDEO, "Failed to compile fake encoder pixel shader");
	}

	return m_shaders[key];
}

VirtualEFBCopy::VirtualEFBCopy()
	: m_hash(0), m_dirty(true)
{ }

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

// FIXME: Are these backwards?
static const float PACK_RG_TO_RG_MATRIX[4*4] = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0
};

static const float PACK_GB_TO_RG_MATRIX[4*4] = {
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 0,
	0, 0, 0, 0
};

static const float ZERO_ADD[4] = { 0, 0, 0, 0 };
static const float A1_ADD[4] = { 0, 0, 0, 1 };

void VirtualEFBCopy::Update(u32 dstAddr, unsigned int dstFormat, D3DTexture2D* srcTex,
	unsigned int srcFormat, const EFBRectangle& srcRect, bool isIntensity,
	bool scaleByHalf)
{
	// Clamp srcRect to 640x528. BPS: The Strike tries to encode an 800x600
	// texture, which is invalid.
	EFBRectangle correctSrc = srcRect;
	correctSrc.ClampUL(0, 0, EFB_WIDTH, EFB_HEIGHT);

	unsigned int newRealW = correctSrc.GetWidth() / (scaleByHalf ? 2 : 1);
	unsigned int newRealH = correctSrc.GetHeight() / (scaleByHalf ? 2 : 1);

	TargetRectangle targetRect = g_renderer->ConvertEFBRectangle(correctSrc);
	unsigned int newVirtualW = targetRect.GetWidth() / (scaleByHalf ? 2 : 1);
	unsigned int newVirtualH = targetRect.GetHeight() / (scaleByHalf ? 2 : 1);

	const float* matrix;
	const float* add;
	DXGI_FORMAT dxFormat;
	switch (dstFormat)
	{
	case EFB_COPY_R4:
	case EFB_COPY_R8_1:
	case EFB_COPY_R8:
		matrix = PACK_R_TO_R_MATRIX;
		add = ZERO_ADD;
		dxFormat = DXGI_FORMAT_R8_UNORM;
		break;
	case EFB_COPY_RA4:
	case EFB_COPY_RA8:
		matrix = PACK_RA_TO_RG_MATRIX;
		add = ZERO_ADD;
		dxFormat = DXGI_FORMAT_R8G8_UNORM;
		break;
	case EFB_COPY_RGB565:
		matrix = RGB0_MATRIX;
		add = A1_ADD;
		dxFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		break;
	case EFB_COPY_RGB5A3:
	case EFB_COPY_RGBA8:
		matrix = RGBA_MATRIX;
		add = ZERO_ADD;
		dxFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		break;
	case EFB_COPY_A8:
		matrix = PACK_A_TO_R_MATRIX;
		add = ZERO_ADD;
		dxFormat = DXGI_FORMAT_R8_UNORM;
		break;
	case EFB_COPY_G8:
		matrix = PACK_G_TO_R_MATRIX;
		add = ZERO_ADD;
		dxFormat = DXGI_FORMAT_R8_UNORM;
		break;
	case EFB_COPY_B8:
		matrix = PACK_B_TO_R_MATRIX;
		add = ZERO_ADD;
		dxFormat = DXGI_FORMAT_R8_UNORM;
		break;
	case EFB_COPY_RG8:
		matrix = PACK_RG_TO_RG_MATRIX;
		add = ZERO_ADD;
		dxFormat = DXGI_FORMAT_R8G8_UNORM;
		break;
	case EFB_COPY_GB8:
		matrix = PACK_GB_TO_RG_MATRIX;
		add = ZERO_ADD;
		dxFormat = DXGI_FORMAT_R8G8_UNORM;
		break;
	}

	EnsureVirtualTexture(newVirtualW, newVirtualH, dxFormat);

	if ((dstFormat == EFB_COPY_RGB5A3 || dstFormat == EFB_COPY_RGBA8) &&
		srcFormat == PIXELFMT_RGBA6_Z24 && !isIntensity && !scaleByHalf)
	{
		// If dst format is rgba, src format is rgba, and no other filters are applied...

		// This combination allows the simple use of CopySubresourceRegion to
		// make a fake base texture.

		DEBUG_LOG(VIDEO, "Updating fake base with CopySubresourceRegion");

		D3D11_BOX srcBox = CD3D11_BOX(targetRect.left, targetRect.top, 0, targetRect.right, targetRect.bottom, 1);
		D3D::g_context->CopySubresourceRegion(m_texture.tex->GetTex(), 0, 0, 0, 0, srcTex->GetTex(), 0, &srcBox);
	}
	else
	{
		VirtualizeShade(srcTex, srcFormat, isIntensity, scaleByHalf,
			targetRect.left, targetRect.top, newVirtualW, newVirtualH,
			matrix, add);
	}

	// Fake base texture was created successfully
	m_realW = newRealW;
	m_realH = newRealH;
	m_dstFormat = dstFormat;
	m_dirty = true;
}

// TODO: Move this stuff to a common place
static const float UNPACK_R_TO_I_MATRIX[16] = {
	1, 0, 0, 0,
	1, 0, 0, 0,
	1, 0, 0, 0,
	1, 0, 0, 0
};

// FIXME: Is this backwards?
static const float UNPACK_RG_TO_IA_MATRIX[16] = {
	1, 0, 0, 0,
	1, 0, 0, 0,
	1, 0, 0, 0,
	0, 1, 0, 0
};

D3DTexture2D* VirtualEFBCopy::Virtualize(u32 ramAddr, u32 width, u32 height,
	u32 levels, u32 format, u32 tlutAddr, u32 tlutFormat, Matrix44& unpackMatrix)
{
	// FIXME: Check if encoded dstFormat and texture format are compatible,
	// reinterpret or fall back to RAM if necessary

	static const char* DST_FORMAT_NAMES[] = {
		"R4", "R8_1", "RA4", "RA8", "RGB565", "RGB5A3", "RGBA8", "A8",
		"R8", "G8", "B8", "RG8", "GB8", "0xD", "0xE", "0xF"
	};

	static const char* TEX_FORMAT_NAMES[] = {
		"I4", "I8", "IA4", "IA8", "RGB565", "RGB5A3", "RGBA8", "0x7",
		"C4", "C8", "C14X2", "0xB", "0xC", "0xD", "CMPR", "0xF"
	};

	INFO_LOG(VIDEO, "Interpreting dstFormat %s as tex format %s at ram addr 0x%.08X",
		DST_FORMAT_NAMES[m_dstFormat], TEX_FORMAT_NAMES[format], ramAddr);

	switch (format)
	{
	case GX_TF_I4:
	case GX_TF_I8:
		Matrix44::Set(unpackMatrix, UNPACK_R_TO_I_MATRIX);
		break;
	case GX_TF_IA4:
	case GX_TF_IA8:
		Matrix44::Set(unpackMatrix, UNPACK_RG_TO_IA_MATRIX);
		break;
	default:
		Matrix44::LoadIdentity(unpackMatrix);
		break;
	}

	return m_texture.tex.get();
}

void VirtualEFBCopy::EnsureVirtualTexture(UINT width, UINT height, DXGI_FORMAT dxFormat)
{
	bool recreate = !m_texture.tex ||
		width != m_texture.width ||
		height != m_texture.height ||
		dxFormat != m_texture.dxFormat;
	if (recreate)
	{
		INFO_LOG(VIDEO, "Recreating virtual texture, size %dx%d", width, height);

		D3D11_TEXTURE2D_DESC t2dd = CD3D11_TEXTURE2D_DESC(dxFormat, width, height,
			1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

		m_texture.tex.reset();
		SharedPtr<ID3D11Texture2D> newFakeBase = CreateTexture2DShared(&t2dd, NULL);
		m_texture.tex.reset(new D3DTexture2D(newFakeBase,
			(D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET)));
		m_texture.width = width;
		m_texture.height = height;
		m_texture.dxFormat = dxFormat;
	}
}

void VirtualEFBCopy::VirtualizeShade(D3DTexture2D* texSrc, unsigned int srcFormat,
	bool yuva, bool scale,
	unsigned int posX, unsigned int posY,
	unsigned int virtualW, unsigned int virtualH,
	const float* colorMatrix, const float* colorAdd)
{
	VirtualCopyShaderManager& virtShaderMan = ((TextureCache*)g_textureCache)->GetVirtShaderManager();
	SharedPtr<ID3D11PixelShader> fakeEncode = virtShaderMan.GetShader(scale, srcFormat == PIXELFMT_Z24);
	if (!fakeEncode)
		return;
	
	DEBUG_LOG(VIDEO, "Doing fake encode shader");

	static const float YUVA_MATRIX[16] = {
		0.257f, 0.504f, 0.098f, 0.f,
		-0.148f, -0.291f, 0.439f, 0.f,
		0.439f, -0.368f, -0.071f, 0.f,
		0.f, 0.f, 0.f, 1.f
	};
	static const float YUV_ADD[3] = { 16.f/255.f, 128.f/255.f, 128.f/255.f };

	SharedPtr<ID3D11Buffer> paramsBuffer = virtShaderMan.GetParams();

	D3D11_MAPPED_SUBRESOURCE map;
	HRESULT hr = D3D::g_context->Map(paramsBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
	if (SUCCEEDED(hr))
	{
		VirtCopyShaderParams* params = (VirtCopyShaderParams*)map.pData;

		// Choose a pre-color matrix: Either RGBA or YUVA
		if (yuva)
		{
			// Combine YUVA matrix with color matrix
			Matrix44 colorMat;
			Matrix44::Set(colorMat, colorMatrix);
			Matrix44 yuvaMat;
			Matrix44::Set(yuvaMat, YUVA_MATRIX);
			Matrix44 combinedMat;
			Matrix44::Multiply(colorMat, yuvaMat, combinedMat);

			memcpy(params->Matrix, combinedMat.data, 4*4*sizeof(float));

			for (int i = 0; i < 3; ++i)
				params->Add[i] = colorAdd[i] + YUV_ADD[i];
			params->Add[3] = colorAdd[3];
		}
		else
		{
			memcpy(params->Matrix, colorMatrix, 4*4*sizeof(float));
			memcpy(params->Add, colorAdd, 4*sizeof(float));
		}

		params->Pos[0] = FLOAT(posX);
		params->Pos[1] = FLOAT(posY);
		params->DisableAlpha = (srcFormat != PIXELFMT_RGBA6_Z24);
		D3D::g_context->Unmap(paramsBuffer, 0);
	}
	else
		ERROR_LOG(VIDEO, "Failed to map fake-encode params buffer");

	// Reset API

	g_renderer->ResetAPIState();
	D3D::stateman->Apply();

	D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, FLOAT(virtualW), FLOAT(virtualH));
	D3D::g_context->RSSetViewports(1, &vp);

	// Re-encode with a different palette

	D3D::g_context->OMSetRenderTargets(1, &m_texture.tex->GetRTV(), NULL);
	D3D::g_context->PSSetConstantBuffers(0, 1, &paramsBuffer);
	D3D::g_context->PSSetShaderResources(0, 1, &texSrc->GetSRV());

	D3D::drawEncoderQuad(fakeEncode);

	IUnknown* nullDummy = NULL;
	D3D::g_context->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&nullDummy);
	D3D::g_context->PSSetConstantBuffers(0, 1, (ID3D11Buffer**)&nullDummy);
	
	// Restore API

	g_renderer->RestoreAPIState();
	D3D::g_context->OMSetRenderTargets(1,
		&FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());
}

}
