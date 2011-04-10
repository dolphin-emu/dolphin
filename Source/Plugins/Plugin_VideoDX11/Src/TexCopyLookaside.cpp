// TexCopyLookaside.cpp
// Nolan Check
// Created 4/4/2011

#include "TexCopyLookaside.h"

#include "FramebufferManager.h"
#include "D3DBase.h"
#include "D3DTexture.h"
#include "D3DShader.h"
#include "Render.h"
#include "TextureDecoder.h"
#include "VertexShaderCache.h"
#include "GfxState.h"
#include "HW/Memmap.h"

namespace DX11
{

struct FakeEncodeParams
{
	FLOAT PreColorMatrix[4][4]; // Matrix to apply to read pixel BEFORE the others
	FLOAT PreColorAdd[4];
	FLOAT ColorMatrix[4][4];
	FLOAT ColorAdd[4];
	FLOAT Pos[2]; // Top-left encode position in virtual texture pixels
};

union FakeEncodeParams_Padded
{
	FakeEncodeParams params;
	// Constant buffers must be a multiple of 16 bytes in size.
	u8 pad[(sizeof(FakeEncodeParams) + 15) & ~15];
};

// TODO: Make part of a class
static SharedPtr<ID3D11PixelShader> s_fakeEncodeShader;
static SharedPtr<ID3D11PixelShader> s_fakeEncodeDepthShader;
static SharedPtr<ID3D11PixelShader> s_fakeEncodeScaleShader;
static SharedPtr<ID3D11Buffer> s_fakeEncodeParams;

static const char FAKE_ENCODE_PS[] =
"// dolphin-emu fake texture encoder pixel shader\n"

"cbuffer cbParams : register(b0)\n"
"{\n"
	"struct\n"
	"{\n"
		"float4x4 PreColorMatrix;\n"
		"float4 PreColorAdd;\n"
		"float4x4 ColorMatrix;\n"
		"float4 ColorAdd;\n"
		"float2 Pos;\n"
	"} Params;\n"
"}\n"

// Use t8 because t0..7 are being used by the vertex manager
"Texture2D<float4> EFBTexture : register(t8);\n"

"void main(out float4 ocol0 : SV_Target, in float4 pos : SV_Position)\n"
"{\n"
	"float4 pixel = EFBTexture.Load(int3(Params.Pos + pos.xy, 0));\n"
	"pixel = mul(pixel, Params.PreColorMatrix) + Params.PreColorAdd;\n"
	"ocol0 = mul(pixel, Params.ColorMatrix) + Params.ColorAdd;\n"
"}\n"
;

static const char FAKE_ENCODE_SCALE_PS[] =
"// dolphin-emu fake texture encoder pixel shader for scaled textures\n"

"cbuffer cbParams : register(b0)\n"
"{\n"
	"struct\n"
	"{\n"
		"float4x4 PreColorMatrix;\n"
		"float4 PreColorAdd;\n"
		"float4x4 ColorMatrix;\n"
		"float4 ColorAdd;\n"
		"float2 Pos;\n"
	"} Params;\n"
"}\n"

// Use t8 because t0..7 are being used by the vertex manager
"Texture2D<float4> EFBTexture : register(t8);\n"

"void main(out float4 ocol0 : SV_Target, in float4 pos : SV_Position)\n"
"{\n"
	"float2 tl = floor(pos.xy);\n"
	
	"float4 pixel0 = EFBTexture.Load(int3(Params.Pos + 2*tl + float2(0,0), 0));\n"
	"pixel0 = mul(pixel0, Params.PreColorMatrix) + Params.PreColorAdd;\n"
	"float4 pixel1 = EFBTexture.Load(int3(Params.Pos + 2*tl + float2(1,0), 0));\n"
	"pixel1 = mul(pixel1, Params.PreColorMatrix) + Params.PreColorAdd;\n"
	"float4 pixel2 = EFBTexture.Load(int3(Params.Pos + 2*tl + float2(0,1), 0));\n"
	"pixel2 = mul(pixel2, Params.PreColorMatrix) + Params.PreColorAdd;\n"
	"float4 pixel3 = EFBTexture.Load(int3(Params.Pos + 2*tl + float2(1,1), 0));\n"
	"pixel3 = mul(pixel3, Params.PreColorMatrix) + Params.PreColorAdd;\n"

	"float4 pixel = 0.25 * (pixel0 + pixel1 + pixel2 + pixel3);\n"

	"ocol0 = mul(pixel, Params.ColorMatrix) + Params.ColorAdd;\n"
"}\n"
;

static const char FAKE_ENCODE_DEPTH_PS[] =
"// dolphin-emu fake texture encoder pixel shader for depth\n"

"cbuffer cbParams : register(b0)\n"
"{\n"
	"struct\n"
	"{\n"
		"float4x4 PreColorMatrix;\n"
		"float4 PreColorAdd;\n"
		"float4x4 ColorMatrix;\n"
		"float4 ColorAdd;\n"
		"float2 Pos;\n"
	"} Params;\n"
"}\n"

// Use t8 because t0..7 are being used by the vertex manager
// FIXME: Can depth textures be loaded like this?
"Texture2D<float4> EFBTexture : register(t8);\n"

"void main(out float4 ocol0 : SV_Target, in float4 pos : SV_Position)\n"
"{\n"
	"float fDepth = EFBTexture.Load(int3(Params.Pos + pos.xy, 0)).r;\n"
	"uint depth24 = 0xFFFFFF * fDepth;\n"
	"uint4 bytes = uint4(\n"
		"(depth24 >> 16) & 0xFF,\n" // r
		"(depth24 >> 8) & 0xFF,\n"  // g
		"depth24 & 0xFF,\n"         // b
		"255);\n"                   // a
	"float4 pixel = bytes / 255.0;\n"
	"pixel = mul(pixel, Params.PreColorMatrix) + Params.PreColorAdd;\n"
	"ocol0 = mul(pixel, Params.ColorMatrix) + Params.ColorAdd;\n"
"}\n"
;

static SharedPtr<ID3D11PixelShader> GetFakeEncodeShader(bool scale, bool depth)
{
	if (!s_fakeEncodeParams)
	{
		D3D11_BUFFER_DESC bd = CD3D11_BUFFER_DESC(sizeof(FakeEncodeParams_Padded),
			D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		s_fakeEncodeParams = CreateBufferShared(&bd, NULL);
		if (!s_fakeEncodeParams)
			ERROR_LOG(VIDEO, "Failed to create fake encode params buffer");
	}

	if (depth)
	{
		if (!s_fakeEncodeDepthShader)
		{
			s_fakeEncodeDepthShader = D3D::CompileAndCreatePixelShader(
				FAKE_ENCODE_DEPTH_PS, sizeof(FAKE_ENCODE_DEPTH_PS));
			if (!s_fakeEncodeDepthShader)
				ERROR_LOG(VIDEO, "Failed to compile fake encoder pixel shader for depth");
		}

		return s_fakeEncodeDepthShader;
	}
	else if (scale)
	{
		if (!s_fakeEncodeScaleShader)
		{
			s_fakeEncodeScaleShader = D3D::CompileAndCreatePixelShader(
				FAKE_ENCODE_SCALE_PS, sizeof(FAKE_ENCODE_SCALE_PS));
			if (!s_fakeEncodeScaleShader)
				ERROR_LOG(VIDEO, "Failed to compile fake encoder pixel shader for scale");
		}

		return s_fakeEncodeScaleShader;
	}
	else
	{
		if (!s_fakeEncodeShader)
		{
			s_fakeEncodeShader = D3D::CompileAndCreatePixelShader(
				FAKE_ENCODE_PS, sizeof(FAKE_ENCODE_PS));
			if (!s_fakeEncodeShader)
				ERROR_LOG(VIDEO, "Failed to compile fake encoder pixel shader");
		}

		return s_fakeEncodeShader;
	}
}

TexCopyLookaside::TexCopyLookaside()
	: m_hash(0)
{ }

static bool IsDstFormatRGB(unsigned int dstFormat)
{
	return dstFormat == 0x4; // RGB565
}

static bool IsDstFormatRGBA(unsigned int dstFormat)
{
	return dstFormat == 0x5 || dstFormat == 0x6; // RGB5A3 or RGBA8
}

static bool IsDstFormatAR(unsigned int dstFormat)
{
	return dstFormat == 0x2 || dstFormat == 0x3; // RA4 or RA8
}

static bool IsDstFormatGR(unsigned int dstFormat)
{
	return dstFormat == 0xB; // GR
}

static bool IsDstFormatBG(unsigned int dstFormat)
{
	return dstFormat == 0xC; // BG
}

static bool IsDstFormatA(unsigned int dstFormat)
{
	return dstFormat == 0x7; // G8
}

static bool IsDstFormatR(unsigned int dstFormat)
{
	return dstFormat == 0x0 || dstFormat == 0x1 || dstFormat == 0x8;
	// R4 or R8 or R8
}

static bool IsDstFormatG(unsigned int dstFormat)
{
	return dstFormat == 0x9; // G8
}

static bool IsDstFormatB(unsigned int dstFormat)
{
	return dstFormat == 0xA; // B8
}

void TexCopyLookaside::Update(u32 dstAddr, unsigned int dstFormat,
	unsigned int srcFormat, const EFBRectangle& srcRect, bool isIntensity,
	bool scaleByHalf)
{
	u64 newHash = m_hash;

	// If Update doesn't succeed, hash will be 0.
	// FIXME: Cleaner way
	m_hash = 0;

	// Clamp srcRect to 640x528. BPS: The Strike tries to encode an 800x600
	// texture, which is invalid.
	EFBRectangle correctSrc = srcRect;
	correctSrc.ClampUL(0, 0, EFB_WIDTH, EFB_HEIGHT);

	unsigned int newRealW = correctSrc.GetWidth() / (scaleByHalf ? 2 : 1);
	unsigned int newRealH = correctSrc.GetHeight() / (scaleByHalf ? 2 : 1);
	
	// NOTE: This code assumes that the real texture has already been encoded to RAM!
	u8* src = Memory::GetPointer(dstAddr);
	// "Preliminary" texture format: Encoded data will most likely be used for
	// this texture format. This also determines the encoded size.
	unsigned int prelimTexFormat;
	switch (dstFormat)
	{
	case 0x0: prelimTexFormat = GX_TF_I4; break; // R4 - TODO: This may be palettized!
	case 0x1: prelimTexFormat = GX_TF_I8; break; // R8 - TODO: This may be palettized!
	case 0x2: prelimTexFormat = GX_TF_IA4; break; // A4 R4
	case 0x3: prelimTexFormat = GX_TF_IA8; break; // A8 R8
	case 0x4: prelimTexFormat = GX_TF_RGB565; break; // R5 G6 B5
	case 0x5: prelimTexFormat = GX_TF_RGB5A3; break; // 1 R5 G5 B5 or 0 A3 R4 G4 B4
	case 0x6: prelimTexFormat = GX_TF_RGBA8; break; // A8 R8 A8 R8 | G8 B8 G8 B8
	case 0x7: prelimTexFormat = GX_TF_I8; break; // A8 - TODO: This may be palettized!
	case 0x8: prelimTexFormat = GX_TF_I8; break; // R8 - TODO: This may be palettized!
	case 0x9: prelimTexFormat = GX_TF_I8; break; // G8 - TODO: This may be palettized!
	case 0xA: prelimTexFormat = GX_TF_I8; break; // B8 - TODO: This may be palettized!
	case 0xB: prelimTexFormat = GX_TF_IA8; break; // G8 R8
	case 0xC: prelimTexFormat = GX_TF_IA8; break; // B8 G8
	default: return;
	}
	u32 sizeInBytes = TexDecoder_GetTextureSizeInBytes(newRealW, newRealH, prelimTexFormat);
	newHash = GetHash64(src, sizeInBytes, sizeInBytes);
	DEBUG_LOG(VIDEO, "Hash of EFB copy at 0x%.08X was taken... 0x%.016X", dstAddr, newHash);

	TargetRectangle targetRect = g_renderer->ConvertEFBRectangle(correctSrc);
	unsigned int newVirtualW = targetRect.GetWidth() / (scaleByHalf ? 2 : 1);
	unsigned int newVirtualH = targetRect.GetHeight() / (scaleByHalf ? 2 : 1);

	bool recreateFakeBase = !m_fakeBase || newVirtualW != m_virtualW || newVirtualH != m_virtualH;
	
	D3DTexture2D* efbTexture = (srcFormat == PIXELFMT_Z24) ?
		FramebufferManager::GetEFBDepthTexture() :
		FramebufferManager::GetEFBColorTexture();

	if (recreateFakeBase)
	{
		INFO_LOG(VIDEO, "Recreating fake base texture, size %dx%d", newVirtualW, newVirtualH);

		D3D11_TEXTURE2D_DESC t2dd = CD3D11_TEXTURE2D_DESC(
			DXGI_FORMAT_R8G8B8A8_UNORM, newVirtualW, newVirtualH,
			1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

		SharedPtr<ID3D11Texture2D> newFakeBase = CreateTexture2DShared(&t2dd, NULL);
		m_fakeBase.reset(new D3DTexture2D(newFakeBase,
			(D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET)));
	}

	if (IsDstFormatRGBA(dstFormat) && srcFormat == PIXELFMT_RGBA6_Z24 && !isIntensity && !scaleByHalf)
	{
		// If dst format is rgba, src format is rgba, and no other filters are applied...

		// This combination allows the simple use of CopySubresourceRegion to
		// make a fake base texture.

		DEBUG_LOG(VIDEO, "Updating fake base with CopySubresourceRegion");

		D3D11_BOX srcBox = CD3D11_BOX(targetRect.left, targetRect.top, 0, targetRect.right, targetRect.bottom, 1);
		D3D::g_context->CopySubresourceRegion(m_fakeBase->GetTex(), 0, 0, 0, 0, efbTexture->GetTex(), 0, &srcBox);
	}
	else if (IsDstFormatRGBA(dstFormat))
	{
		// Copy rgba

		float colorMatrix[4][4] = {
			{ 1, 0, 0, 0 },
			{ 0, 1, 0, 0 },
			{ 0, 0, 1, 0 },
			{ 0, 0, 0, 1 }
		};
		float colorAdd[4] = { 0, 0, 0, 0 };

		FakeEncodeShade(efbTexture, srcFormat, isIntensity, scaleByHalf,
			targetRect.left, targetRect.top, newVirtualW, newVirtualH,
			&colorMatrix[0][0], colorAdd);
	}
	else if (IsDstFormatRGB(dstFormat))
	{
		// Copy rgb, set a to 1

		float colorMatrix[4][4] = {
			{ 1, 0, 0, 0 },
			{ 0, 1, 0, 0 },
			{ 0, 0, 1, 0 },
			{ 0, 0, 0, 0 }
		};
		float colorAdd[4] = { 0, 0, 0, 1 };

		FakeEncodeShade(efbTexture, srcFormat, isIntensity, scaleByHalf,
			targetRect.left, targetRect.top, newVirtualW, newVirtualH,
			&colorMatrix[0][0], colorAdd);
	}
	else if (IsDstFormatAR(dstFormat))
	{
		// Replicate r to all color channels, copy a to alpha

		float colorMatrix[4][4] = {
			{ 1, 0, 0, 0 },
			{ 1, 0, 0, 0 },
			{ 1, 0, 0, 0 },
			{ 0, 0, 0, 1 }
		};
		float colorAdd[4] = { 0, 0, 0, 0 };

		FakeEncodeShade(efbTexture, srcFormat, isIntensity, scaleByHalf,
			targetRect.left, targetRect.top, newVirtualW, newVirtualH,
			&colorMatrix[0][0], colorAdd);
	}
	else if (IsDstFormatGR(dstFormat))
	{
		// Replicate r to all color channels, copy g to alpha
		// FIXME: Is this a good way?

		float colorMatrix[4][4] = {
			{ 1, 0, 0, 0 },
			{ 1, 0, 0, 0 },
			{ 1, 0, 0, 0 },
			{ 0, 1, 0, 0 }
		};
		float colorAdd[4] = { 0, 0, 0, 0 };

		FakeEncodeShade(efbTexture, srcFormat, isIntensity, scaleByHalf,
			targetRect.left, targetRect.top, newVirtualW, newVirtualH,
			&colorMatrix[0][0], colorAdd);
	}
	else if (IsDstFormatBG(dstFormat))
	{
		// Replicate g to all color channels, copy b to alpha
		// FIXME: Is this a good way?

		float colorMatrix[4][4] = {
			{ 0, 1, 0, 0 },
			{ 0, 1, 0, 0 },
			{ 0, 1, 0, 0 },
			{ 0, 0, 1, 0 }
		};
		float colorAdd[4] = { 0, 0, 0, 0 };

		FakeEncodeShade(efbTexture, srcFormat, isIntensity, scaleByHalf,
			targetRect.left, targetRect.top, newVirtualW, newVirtualH,
			&colorMatrix[0][0], colorAdd);
	}
	else if (IsDstFormatA(dstFormat))
	{
		// Replicate a to all channels

		float colorMatrix[4][4] = {
			{ 0, 0, 0, 1 },
			{ 0, 0, 0, 1 },
			{ 0, 0, 0, 1 },
			{ 0, 0, 0, 1 }
		};
		float colorAdd[4] = { 0, 0, 0, 0 };

		FakeEncodeShade(efbTexture, srcFormat, isIntensity, scaleByHalf,
			targetRect.left, targetRect.top, newVirtualW, newVirtualH,
			&colorMatrix[0][0], colorAdd);
	}
	else if (IsDstFormatR(dstFormat))
	{
		// Replicate r to all channels

		float colorMatrix[4][4] = {
			{ 1, 0, 0, 0 },
			{ 1, 0, 0, 0 },
			{ 1, 0, 0, 0 },
			{ 1, 0, 0, 0 }
		};
		float colorAdd[4] = { 0, 0, 0, 0 };

		FakeEncodeShade(efbTexture, srcFormat, isIntensity, scaleByHalf,
			targetRect.left, targetRect.top, newVirtualW, newVirtualH,
			&colorMatrix[0][0], colorAdd);
	}
	else if (IsDstFormatG(dstFormat))
	{
		// Replicate g to all channels

		float colorMatrix[4][4] = {
			{ 0, 1, 0, 0 },
			{ 0, 1, 0, 0 },
			{ 0, 1, 0, 0 },
			{ 0, 1, 0, 0 }
		};
		float colorAdd[4] = { 0, 0, 0, 0 };

		FakeEncodeShade(efbTexture, srcFormat, isIntensity, scaleByHalf,
			targetRect.left, targetRect.top, newVirtualW, newVirtualH,
			&colorMatrix[0][0], colorAdd);
	}
	else if (IsDstFormatB(dstFormat))
	{
		// Replicate b to all channels

		float colorMatrix[4][4] = {
			{ 0, 0, 1, 0 },
			{ 0, 0, 1, 0 },
			{ 0, 0, 1, 0 },
			{ 0, 0, 1, 0 }
		};
		float colorAdd[4] = { 0, 0, 0, 0 };

		FakeEncodeShade(efbTexture, srcFormat, isIntensity, scaleByHalf,
			targetRect.left, targetRect.top, newVirtualW, newVirtualH,
			&colorMatrix[0][0], colorAdd);
	}
	else
	{
		ERROR_LOG(VIDEO, "Not implemented: Cannot fake this combination of formats");
		return;
	}

	// Fake base texture was created successfully
	m_realW = newRealW;
	m_realH = newRealH;
	m_virtualW = newVirtualW;
	m_virtualH = newVirtualH;
	m_dstFormat = dstFormat;
	m_hash = newHash;
}

D3DTexture2D* TexCopyLookaside::FakeTexture(u32 ramAddr, u32 width, u32 height,
	u32 levels, u32 format, u32 tlutAddr, u32 tlutFormat)
{
	// FIXME: Check if encoded dstFormat and texture format are compatible,
	// reinterpret or fall back to RAM if necessary

	static const char* DST_FORMAT_NAMES[] = {
		"R4", "0x1", "RA4", "RA8", "RGB565", "RGB5A3", "RGBA8", "A8",
		"R8", "G8", "B8", "RG8", "GB8", "0xD", "0xE", "0xF"
	};

	static const char* TEX_FORMAT_NAMES[] = {
		"I4", "I8", "IA4", "IA8", "RGB565", "RGB5A3", "RGBA8", "0x7",
		"C4", "C8", "C14X2", "0xB", "0xC", "0xD", "CMPR", "0xF"
	};

	INFO_LOG(VIDEO, "Interpreting dstFormat %s as tex format %s",
		DST_FORMAT_NAMES[m_dstFormat], TEX_FORMAT_NAMES[format]);
	
	// FIXME: How do we handle levels > 1?
	if (!m_hash || width != m_realW || height != m_realH || levels != 1)
		return NULL;

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

	static const float RGB_MATRIX[3][4] = {
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 }
	};
	static const float RGB_ADD[3] = { 0, 0, 0 };

	static const float YUV_MATRIX[3][4] = {
		{ 0.257f, 0.504f, 0.098f, 0 },
		{ -0.148f, -0.291f, 0.439f, 0 },
		{ 0.439f, -0.368f, -0.071f, 0 }
	};
	static const float YUV_ADD[3] = { 16.0f/255.0f, 128.0f/255.0f, 128.0f/255.0f };

	D3D11_MAPPED_SUBRESOURCE map;
	HRESULT hr = D3D::g_context->Map(s_fakeEncodeParams, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
	if (SUCCEEDED(hr))
	{
		FakeEncodeParams* params = (FakeEncodeParams*)map.pData;

		// Choose a pre-color matrix: Either RGBA or YUVA
		if (yuva)
		{
			memcpy(params->PreColorMatrix, YUV_MATRIX, sizeof(YUV_MATRIX));
			memcpy(params->PreColorAdd, YUV_ADD, sizeof(YUV_ADD));
		}
		else
		{
			memcpy(params->PreColorMatrix, RGB_MATRIX, sizeof(RGB_MATRIX));
			memcpy(params->PreColorAdd, RGB_ADD, sizeof(RGB_ADD));
		}

		// If src format has no alpha channel, adjust pre-matrix to set alpha
		// to 1
		if (srcFormat != PIXELFMT_RGBA6_Z24)
		{
			// No alpha channel
			params->PreColorMatrix[3][0] = params->PreColorMatrix[3][1] =
				params->PreColorMatrix[3][2] = params->PreColorMatrix[3][3] = 0;
			params->PreColorAdd[3] = 1;
		}
		else
		{
			// Alpha channel
			params->PreColorMatrix[3][0] = params->PreColorMatrix[3][1] =
				params->PreColorMatrix[3][2] = 0;
			params->PreColorMatrix[3][3] = 1;
			params->PreColorAdd[3] = 0;
		}

		memcpy(params->ColorMatrix, colorMatrix, 4*4*sizeof(float));
		memcpy(params->ColorAdd, colorAdd, 4*sizeof(float));
		params->Pos[0] = FLOAT(posX);
		params->Pos[1] = FLOAT(posY);
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
