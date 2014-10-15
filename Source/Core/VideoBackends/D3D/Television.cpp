// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <vector>

#include "Core/HW/Memmap.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DShader.h"
#include "VideoBackends/D3D/D3DUtil.h"
#include "VideoBackends/D3D/Television.h"
#include "VideoBackends/D3D/VertexShaderCache.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{

static const char YUYV_DECODER_PS[] =
"// dolphin-emu YUYV decoder pixel shader\n"

"Texture2D Tex0 : register(t0);\n"
"sampler Samp0 : register(s0);\n"

"static const float3x3 YCBCR_TO_RGB = float3x3(\n"
	"1.164, 0.000, 1.596,\n"
	"1.164, -0.392, -0.813,\n"
	"1.164, 2.017, 0.000\n"
	");\n"

"void main(out float4 ocol0 : SV_Target, in float4 pos : SV_Position, in float2 uv0 : TEXCOORD0)\n"
"{\n"
	"float3 sample = Tex0.Sample(Samp0, uv0).rgb;\n"

	// GameCube/Wii XFB data is in YUYV format with ITU-R Rec. BT.601 color
	// primaries, compressed to the range Y in 16..235, U and V in 16..240.
	// We want to convert it to RGB format with sRGB color primaries, with
	// range 0..255.

	// Recover RGB components
	"float3 yuv_601_sub = sample.grb - float3(16.0/255.0, 128.0/255.0, 128.0/255.0);\n"
	"float3 rgb_601 = mul(YCBCR_TO_RGB, yuv_601_sub);\n"

	// If we were really obsessed with accuracy, we would correct for the
	// differing color primaries between BT.601 and sRGB. However, this may not
	// be worth the trouble because:
	// - BT.601 defines two sets of primaries: one for NTSC and one for PAL.
	// - sRGB's color primaries are actually an intermediate between BT.601's
	//   NTSC and PAL primaries.
	// - If users even noticed any difference at all, they would be confused by
	//   the slightly-different colors in the NTSC and PAL versions of the same
	//   game.
	// - Even the game designers probably don't pay close attention to this
	//   stuff.
	// Still, instructions on how to do it can be found at
	// <http://www.poynton.com/notes/colour_and_gamma/ColorFAQ.html#RTFToC20>

	"ocol0 = float4(rgb_601, 1);\n"
"}\n"
;

Television::Television()
	: m_yuyvTexture(nullptr), m_yuyvTextureSRV(nullptr), m_pShader(nullptr)
{ }

void Television::Init()
{
	HRESULT hr;

	// Create YUYV texture for real XFB mode


	// Initialize the texture with YCbCr black
	//
	// Some games use narrower XFB widths (Nintendo titles are fond of 608),
	// so the sampler's BorderColor won't cover the right side
	// (see sampler state below)
	const unsigned int MAX_XFB_SIZE = 2*(MAX_XFB_WIDTH) * MAX_XFB_HEIGHT;
	std::vector<u8> fill(MAX_XFB_SIZE);
	for (size_t i = 0; i < MAX_XFB_SIZE / sizeof(u32); ++i)
		reinterpret_cast<u32*>(fill.data())[i] = 0x80108010;
	D3D11_SUBRESOURCE_DATA srd = { fill.data(), 2*(MAX_XFB_WIDTH), 0 };

	// This texture format is designed for YUYV data.
	D3D11_TEXTURE2D_DESC t2dd = CD3D11_TEXTURE2D_DESC(
		DXGI_FORMAT_G8R8_G8B8_UNORM, MAX_XFB_WIDTH, MAX_XFB_HEIGHT, 1, 1);
	hr = D3D::device->CreateTexture2D(&t2dd, &srd, &m_yuyvTexture);
	CHECK(SUCCEEDED(hr), "create tv yuyv texture");
	D3D::SetDebugObjectName(m_yuyvTexture, "tv yuyv texture");

	// Create shader resource view for YUYV texture

	D3D11_SHADER_RESOURCE_VIEW_DESC srvd = CD3D11_SHADER_RESOURCE_VIEW_DESC(
		m_yuyvTexture, D3D11_SRV_DIMENSION_TEXTURE2D,
		DXGI_FORMAT_G8R8_G8B8_UNORM);
	hr = D3D::device->CreateShaderResourceView(m_yuyvTexture, &srvd, &m_yuyvTextureSRV);
	CHECK(SUCCEEDED(hr), "create tv yuyv texture srv");
	D3D::SetDebugObjectName(m_yuyvTextureSRV, "tv yuyv texture srv");

	// Create YUYV-decoding pixel shader

	m_pShader = D3D::CompileAndCreatePixelShader(YUYV_DECODER_PS);
	CHECK(m_pShader != nullptr, "compile and create yuyv decoder pixel shader");
	D3D::SetDebugObjectName(m_pShader, "yuyv decoder pixel shader");

	// Create sampler state and set border color
	//
	// The default sampler border color of { 0.f, 0.f, 0.f, 0.f }
	// creates a green border around the image - see issue 6483
	// (remember, the XFB is being interpreted as YUYV, and 0,0,0,0
	// is actually two green pixels in YUYV - black should be 16,128,16,128,
	// but we reverse the order to match DXGI_FORMAT_G8R8_G8B8_UNORM's ordering)
	float border[4] = { 128.0f/255.0f, 16.0f/255.0f, 128.0f/255.0f, 16.0f/255.0f };
	D3D11_SAMPLER_DESC samDesc = CD3D11_SAMPLER_DESC(D3D11_FILTER_MIN_MAG_MIP_LINEAR,
		D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_BORDER,
		0.f, 1, D3D11_COMPARISON_ALWAYS, border, 0.f, 0.f);
	hr = D3D::device->CreateSamplerState(&samDesc, &m_samplerState);
	CHECK(SUCCEEDED(hr), "create yuyv decoder sampler state");
	D3D::SetDebugObjectName(m_samplerState, "yuyv decoder sampler state");
}

void Television::Shutdown()
{
	SAFE_RELEASE(m_pShader);
	SAFE_RELEASE(m_yuyvTextureSRV);
	SAFE_RELEASE(m_yuyvTexture);
	SAFE_RELEASE(m_samplerState);
}

void Television::Submit(u32 xfbAddr, u32 stride, u32 width, u32 height)
{
	m_curAddr = xfbAddr;
	m_curWidth = width;
	m_curHeight = height;

	// Load data from GameCube RAM to YUYV texture
	u8* yuyvSrc = Memory::GetPointer(xfbAddr);
	D3D11_BOX box = CD3D11_BOX(0, 0, 0, stride, height, 1);
	D3D::context->UpdateSubresource(m_yuyvTexture, 0, &box, yuyvSrc, 2*stride, 2*stride*height);
}

void Television::Render()
{
	if (g_ActiveConfig.bUseRealXFB && g_ActiveConfig.bUseXFB)
	{
		// Use real XFB mode
		// TODO: If this is the lower field, render at a vertical offset of 1
		// line down. We could even consider implementing a deinterlacing
		// algorithm.

		MathUtil::Rectangle<int> sourceRc(0, 0, int(m_curWidth), int(m_curHeight));
		MathUtil::Rectangle<float> destRc(-1.f, 1.f, 1.f, -1.f);

		D3D::context->PSSetSamplers(0, 1, &m_samplerState);

		D3D::drawShadedTexSubQuad(
			m_yuyvTextureSRV, &sourceRc,
			MAX_XFB_WIDTH, MAX_XFB_HEIGHT,
			&destRc,
			m_pShader,
			VertexShaderCache::GetSimpleVertexShader(),
			VertexShaderCache::GetSimpleInputLayout());
	}
	else if (g_ActiveConfig.bUseXFB)
	{
		// Use virtual XFB mode

		// TODO: Eventually, Television should render the Virtual XFB mode
		// display as well.
	}
}

}
