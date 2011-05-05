// TexCopyLookaside.cpp
// Nolan Check
// Created 4/4/2011

#include "TexCopyLookaside.h"

#include "FramebufferManager.h"
#include "D3DBase.h"
#include "D3DTexture.h"
#include "D3DShader.h"
#include "EFBCopy.h"
#include "Render.h"
#include "TextureDecoder.h"
#include "VertexShaderCache.h"
#include "GfxState.h"
#include "HW/Memmap.h"

namespace DX11
{

struct FakeEncodeParams
{
	FLOAT Matrix[4][4];
	FLOAT Add[4];
	FLOAT Pos[2]; // Top-left of source in texels
	BOOL DisableAlpha; // If true, alpha will read as 1 from the source
};

union FakeEncodeParams_Padded
{
	FakeEncodeParams params;
	// Constant buffers must be a multiple of 16 bytes in size.
	u8 pad[(sizeof(FakeEncodeParams) + 15) & ~15];
};

// TODO: Make part of a class
static SharedPtr<ID3D11PixelShader> s_fakeEncodeShaders[4];
static SharedPtr<ID3D11Buffer> s_fakeEncodeParams;

static const char FAKE_ENCODE_PS[] =
"// dolphin-emu fake texture encoder pixel shader\n"

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

// Use t8 because t0..7 are being used by the vertex manager
"Texture2D<float4> EFBTexture : register(t8);\n"

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

static SharedPtr<ID3D11PixelShader> GetFakeEncodeShader(bool scale, bool depth)
{
	// There are 4 different combinations of scale and depth.
	int key = (depth ? (1<<1) : 0) | (scale ? 1 : 0);

	if (!s_fakeEncodeParams)
	{
		D3D11_BUFFER_DESC bd = CD3D11_BUFFER_DESC(sizeof(FakeEncodeParams_Padded),
			D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		s_fakeEncodeParams = CreateBufferShared(&bd, NULL);
		if (!s_fakeEncodeParams)
			ERROR_LOG(VIDEO, "Failed to create fake encode params buffer");
	}

	if (!s_fakeEncodeShaders[key])
	{
		D3D_SHADER_MACRO macros[] = {
			{ "DEPTH", depth ? "1" : "0" },
			{ "SCALE", scale ? "1" : "0" },
			{ NULL, NULL }
		};

		s_fakeEncodeShaders[key] = D3D::CompileAndCreatePixelShader(
			FAKE_ENCODE_PS, sizeof(FAKE_ENCODE_PS), macros);
		if (!s_fakeEncodeShaders[key])
			ERROR_LOG(VIDEO, "Failed to compile fake encoder pixel shader");
	}

	return s_fakeEncodeShaders[key];
}

TexCopyLookaside::TexCopyLookaside()
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

static const float RRRA_MATRIX[4*4] = {
	1, 0, 0, 0,
	1, 0, 0, 0,
	1, 0, 0, 0,
	0, 0, 0, 1
};

static const float AAAA_MATRIX[4*4] = {
	0, 0, 0, 1,
	0, 0, 0, 1,
	0, 0, 0, 1,
	0, 0, 0, 1
};

static const float RRRR_MATRIX[4*4] = {
	1, 0, 0, 0,
	1, 0, 0, 0,
	1, 0, 0, 0,
	1, 0, 0, 0
};

static const float GGGG_MATRIX[4*4] = {
	0, 1, 0, 0,
	0, 1, 0, 0,
	0, 1, 0, 0,
	0, 1, 0, 0
};

static const float BBBB_MATRIX[4*4] = {
	0, 0, 1, 0,
	0, 0, 1, 0,
	0, 0, 1, 0,
	0, 0, 1, 0
};

static const float RRRG_MATRIX[4*4] = {
	1, 0, 0, 0,
	1, 0, 0, 0,
	1, 0, 0, 0,
	0, 1, 0, 0
};

static const float GGGB_MATRIX[4*4] = {
	0, 1, 0, 0,
	0, 1, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0
};

static const float ZERO_ADD[4] = { 0, 0, 0, 0 };
static const float A1_ADD[4] = { 0, 0, 0, 1 };

void TexCopyLookaside::Update(u32 dstAddr, unsigned int dstFormat,
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

	bool recreateFakeBase = !m_fakeBase || newVirtualW != m_virtualW || newVirtualH != m_virtualH;
	
	D3DTexture2D* efbTexture = (srcFormat == PIXELFMT_Z24) ?
		FramebufferManager::GetEFBDepthTexture() :
	// FIXME: Resolving EFB here will cause minor artifacts if the texture is
	// interpreted with a palette. On DX10.1+ hardware, it would be better to
	// copy into a multisampled fake-base, have the depalettizer work with
	// multisampled textures, then resolve after depalettization.
		FramebufferManager::GetResolvedEFBColorTexture();

	if (recreateFakeBase)
	{
		INFO_LOG(VIDEO, "Recreating fake base texture, size %dx%d", newVirtualW, newVirtualH);

		D3D11_TEXTURE2D_DESC t2dd = CD3D11_TEXTURE2D_DESC(
			DXGI_FORMAT_R8G8B8A8_UNORM, newVirtualW, newVirtualH,
			1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

		m_fakeBase.reset();
		SharedPtr<ID3D11Texture2D> newFakeBase = CreateTexture2DShared(&t2dd, NULL);
		m_fakeBase.reset(new D3DTexture2D(newFakeBase,
			(D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET)));
	}

	if ((dstFormat == EFB_COPY_RGB5A3 || dstFormat == EFB_COPY_RGBA8) &&
		srcFormat == PIXELFMT_RGBA6_Z24 && !isIntensity && !scaleByHalf)
	{
		// If dst format is rgba, src format is rgba, and no other filters are applied...

		// This combination allows the simple use of CopySubresourceRegion to
		// make a fake base texture.

		DEBUG_LOG(VIDEO, "Updating fake base with CopySubresourceRegion");

		D3D11_BOX srcBox = CD3D11_BOX(targetRect.left, targetRect.top, 0, targetRect.right, targetRect.bottom, 1);
		D3D::g_context->CopySubresourceRegion(m_fakeBase->GetTex(), 0, 0, 0, 0, efbTexture->GetTex(), 0, &srcBox);
	}
	else
	{
		const float* colorMatrix;
		const float* colorAdd = ZERO_ADD;

		switch (dstFormat)
		{
		case EFB_COPY_R4:
		case EFB_COPY_R8_1:
		case EFB_COPY_R8:
			colorMatrix = RRRR_MATRIX;
			break;
		case EFB_COPY_RA4:
		case EFB_COPY_RA8:
			colorMatrix = RRRA_MATRIX;
			break;
		case EFB_COPY_RGB565:
			colorMatrix = RGB0_MATRIX;
			colorAdd = A1_ADD;
			break;
		case EFB_COPY_RGB5A3:
		case EFB_COPY_RGBA8:
			colorMatrix = RGBA_MATRIX;
			break;
		case EFB_COPY_A8:
			colorMatrix = AAAA_MATRIX;
			break;
		case EFB_COPY_G8:
			colorMatrix = GGGG_MATRIX;
			break;
		case EFB_COPY_B8:
			colorMatrix = BBBB_MATRIX;
			break;
		case EFB_COPY_RG8:
			colorMatrix = RRRG_MATRIX;
			break;
		case EFB_COPY_GB8:
			colorMatrix = GGGB_MATRIX;
			break;
		default:
			ERROR_LOG(VIDEO, "Couldn't fake this EFB copy format 0x%X", dstFormat);
			m_fakeBase.reset();
			return;
		}

		FakeEncodeShade(efbTexture, srcFormat, isIntensity, scaleByHalf,
			targetRect.left, targetRect.top, newVirtualW, newVirtualH,
			colorMatrix, colorAdd);
	}

	// Fake base texture was created successfully
	m_realW = newRealW;
	m_realH = newRealH;
	m_virtualW = newVirtualW;
	m_virtualH = newVirtualH;
	m_dstFormat = dstFormat;
	m_dirty = true;
}

D3DTexture2D* TexCopyLookaside::FakeTexture(u32 ramAddr, u32 width, u32 height,
	u32 levels, u32 format, u32 tlutAddr, u32 tlutFormat)
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

	INFO_LOG(VIDEO, "Interpreting dstFormat %s as tex format %s",
		DST_FORMAT_NAMES[m_dstFormat], TEX_FORMAT_NAMES[format]);

	return m_fakeBase.get();
}

void TexCopyLookaside::FakeEncodeShade(D3DTexture2D* texSrc, unsigned int srcFormat,
	bool yuva, bool scale,
	unsigned int posX, unsigned int posY,
	unsigned int virtualW, unsigned int virtualH,
	const float* colorMatrix, const float* colorAdd)
{
	SharedPtr<ID3D11PixelShader> fakeEncode = GetFakeEncodeShader(scale, srcFormat == PIXELFMT_Z24);
	if (!fakeEncode)
		return;
	
	DEBUG_LOG(VIDEO, "Doing fake encode shader");

	static const float YUVA_MATRIX[4][4] = {
		{ 0.257f, 0.504f, 0.098f, 0.f },
		{ -0.148f, -0.291f, 0.439f, 0.f },
		{ 0.439f, -0.368f, -0.071f, 0.f },
		{ 0.f, 0.f, 0.f, 1.f }
	};
	static const float YUV_ADD[3] = { 16.f/255.f, 128.f/255.f, 128.f/255.f };

	D3D11_MAPPED_SUBRESOURCE map;
	HRESULT hr = D3D::g_context->Map(s_fakeEncodeParams, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
	if (SUCCEEDED(hr))
	{
		FakeEncodeParams* params = (FakeEncodeParams*)map.pData;

		// Choose a pre-color matrix: Either RGBA or YUVA
		if (yuva)
		{
			// Combine yuva matrix with colorMatrix
			for (int row = 0; row < 4; ++row)
			{
				for (int col = 0; col < 4; ++col)
				{
					params->Matrix[row][col] =
						colorMatrix[4*row+0] * YUVA_MATRIX[0][col] +
						colorMatrix[4*row+1] * YUVA_MATRIX[1][col] +
						colorMatrix[4*row+2] * YUVA_MATRIX[2][col] +
						colorMatrix[4*row+3] * YUVA_MATRIX[3][col];
				}
			}
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
		D3D::g_context->Unmap(s_fakeEncodeParams, 0);
	}
	else
		ERROR_LOG(VIDEO, "Failed to map fake-encode params buffer");

	// Reset API

	g_renderer->ResetAPIState();
	D3D::stateman->Apply();

	D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, FLOAT(virtualW), FLOAT(virtualH));
	D3D::g_context->RSSetViewports(1, &vp);

	// Re-encode with a different palette

	D3D::g_context->OMSetRenderTargets(1, &m_fakeBase->GetRTV(), NULL);
	D3D::g_context->PSSetConstantBuffers(0, 1, &s_fakeEncodeParams);
	D3D::g_context->PSSetShaderResources(8, 1, &texSrc->GetSRV());

	D3D::drawEncoderQuad(fakeEncode);

	IUnknown* nullDummy = NULL;
	D3D::g_context->PSSetShaderResources(8, 1, (ID3D11ShaderResourceView**)&nullDummy);
	D3D::g_context->PSSetConstantBuffers(0, 1, (ID3D11Buffer**)&nullDummy);
	
	// Restore API

	g_renderer->RestoreAPIState();
	D3D::g_context->OMSetRenderTargets(1,
		&FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());
}

}
