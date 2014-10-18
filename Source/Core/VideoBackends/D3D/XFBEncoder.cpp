// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DBlob.h"
#include "VideoBackends/D3D/D3DShader.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/FramebufferManager.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3D/XFBEncoder.h"

namespace DX11
{

union XFBEncodeParams
{
	struct
	{
		FLOAT Width; // Width and height of encoded XFB in luma pixels
		FLOAT Height;
		FLOAT TexLeft; // Normalized tex coordinates of XFB source area in EFB texture
		FLOAT TexTop;
		FLOAT TexRight;
		FLOAT TexBottom;
		FLOAT Gamma;
	};
	// Constant buffers must be a multiple of 16 bytes in size
	u8 pad[32]; // Pad to the next multiple of 16
};

static const char XFB_ENCODE_VS[] =
"// dolphin-emu XFB encoder vertex shader\n"

"cbuffer cbParams : register(b0)\n"
"{\n"
	"struct\n" // Should match XFBEncodeParams above
	"{\n"
		"float Width;\n"
		"float Height;\n"
		"float TexLeft;\n"
		"float TexTop;\n"
		"float TexRight;\n"
		"float TexBottom;\n"
		"float Gamma;\n"
	"} Params;\n"
"}\n"

"struct Output\n"
"{\n"
	"float4 Pos : SV_Position;\n"
	"float2 Coord : ENCODECOORD;\n"
"};\n"

"Output main(in float2 Pos : POSITION)\n"
"{\n"
	"Output result;\n"
	"result.Pos = float4(2*Pos.x-1, -2*Pos.y+1, 0, 1);\n"
	"result.Coord = Pos * float2(floor(Params.Width/2), Params.Height);\n"
	"return result;\n"
"}\n"
;

static const char XFB_ENCODE_PS[] =
"// dolphin-emu XFB encoder pixel shader\n"

"cbuffer cbParams : register(b0)\n"
"{\n"
	"struct\n" // Should match XFBEncodeParams above
	"{\n"
		"float Width;\n"
		"float Height;\n"
		"float TexLeft;\n"
		"float TexTop;\n"
		"float TexRight;\n"
		"float TexBottom;\n"
		"float Gamma;\n"
	"} Params;\n"
"}\n"

"Texture2D EFBTexture : register(t0);\n"
"sampler EFBSampler : register(s0);\n"

// GameCube/Wii uses the BT.601 standard algorithm for converting to YCbCr; see
// <http://www.equasys.de/colorconversion.html#YCbCr-RGBColorFormatConversion>
"static const float3x4 RGB_TO_YCBCR = float3x4(\n"
	"0.257, 0.504, 0.098, 16.0/255.0,\n"
	"-0.148, -0.291, 0.439, 128.0/255.0,\n"
	"0.439, -0.368, -0.071, 128.0/255.0\n"
	");\n"

"float3 SampleEFB(float2 coord)\n"
"{\n"
	"float2 texCoord = lerp(float2(Params.TexLeft,Params.TexTop), float2(Params.TexRight,Params.TexBottom), coord / float2(Params.Width,Params.Height));\n"
	"return EFBTexture.Sample(EFBSampler, texCoord).rgb;\n"
"}\n"

"void main(out float4 ocol0 : SV_Target, in float4 Pos : SV_Position, in float2 Coord : ENCODECOORD)\n"
"{\n"
	// Multiplying X by 2, moves pixel centers from (x+0.5) to (2x+1) instead of (2x+0.5), so subtract 0.5 to compensate
	"float2 baseCoord = Coord * float2(2,1) - float2(0.5,0);\n"
	// FIXME: Shall we apply gamma here, or apply it below to the Y components?
	// Be careful if you apply it to Y! The Y components are in the range (16..235) / 255.
	"float3 sampleL = pow(abs(SampleEFB(baseCoord+float2(-1,0))), Params.Gamma);\n" // Left
	"float3 sampleM = pow(abs(SampleEFB(baseCoord)), Params.Gamma);\n" // Middle
	"float3 sampleR = pow(abs(SampleEFB(baseCoord+float2(1,0))), Params.Gamma);\n" // Right

	"float3 yuvL = mul(RGB_TO_YCBCR, float4(sampleL,1));\n"
	"float3 yuvM = mul(RGB_TO_YCBCR, float4(sampleM,1));\n"
	"float3 yuvR = mul(RGB_TO_YCBCR, float4(sampleR,1));\n"

	// The Y components correspond to two EFB pixels, while the U and V are
	// made from a blend of three EFB pixels.
	"float y0 = yuvM.r;\n"
	"float y1 = yuvR.r;\n"
	"float u0 = 0.25*yuvL.g + 0.5*yuvM.g + 0.25*yuvR.g;\n"
	"float v0 = 0.25*yuvL.b + 0.5*yuvM.b + 0.25*yuvR.b;\n"

	"ocol0 = float4(y0, u0, y1, v0);\n"
"}\n"
;

static const D3D11_INPUT_ELEMENT_DESC QUAD_LAYOUT_DESC[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const struct QuadVertex
{
	float posX;
	float posY;
} QUAD_VERTS[4] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };

XFBEncoder::XFBEncoder()
	: m_out(nullptr), m_outRTV(nullptr), m_outStage(nullptr), m_encodeParams(nullptr),
	m_quad(nullptr), m_vShader(nullptr), m_quadLayout(nullptr), m_pShader(nullptr),
	m_xfbEncodeBlendState(nullptr), m_xfbEncodeDepthState(nullptr),
	m_xfbEncodeRastState(nullptr), m_efbSampler(nullptr)
{ }

void XFBEncoder::Init()
{
	HRESULT hr;

	// Create output texture

	// The pixel shader can generate one YUYV entry per pixel. One YUYV entry
	// is created for every two EFB pixels.
	D3D11_TEXTURE2D_DESC t2dd = CD3D11_TEXTURE2D_DESC(
		DXGI_FORMAT_R8G8B8A8_UNORM, MAX_XFB_WIDTH/2, MAX_XFB_HEIGHT, 1, 1,
		D3D11_BIND_RENDER_TARGET);
	hr = D3D::device->CreateTexture2D(&t2dd, nullptr, &m_out);
	CHECK(SUCCEEDED(hr), "create xfb encoder output texture");
	D3D::SetDebugObjectName(m_out, "xfb encoder output texture");

	// Create output render target view

	D3D11_RENDER_TARGET_VIEW_DESC rtvd = CD3D11_RENDER_TARGET_VIEW_DESC(m_out,
		D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM);
	hr = D3D::device->CreateRenderTargetView(m_out, &rtvd, &m_outRTV);
	CHECK(SUCCEEDED(hr), "create xfb encoder output texture rtv");
	D3D::SetDebugObjectName(m_outRTV, "xfb encoder output rtv");

	// Create output staging buffer

	t2dd.Usage = D3D11_USAGE_STAGING;
	t2dd.BindFlags = 0;
	t2dd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	hr = D3D::device->CreateTexture2D(&t2dd, nullptr, &m_outStage);
	CHECK(SUCCEEDED(hr), "create xfb encoder output staging buffer");
	D3D::SetDebugObjectName(m_outStage, "xfb encoder output staging buffer");

	// Create constant buffer for uploading params to shaders

	D3D11_BUFFER_DESC bd = CD3D11_BUFFER_DESC(sizeof(XFBEncodeParams),
		D3D11_BIND_CONSTANT_BUFFER);
	hr = D3D::device->CreateBuffer(&bd, nullptr, &m_encodeParams);
	CHECK(SUCCEEDED(hr), "create xfb encode params buffer");
	D3D::SetDebugObjectName(m_encodeParams, "xfb encoder params buffer");

	// Create vertex quad

	bd = CD3D11_BUFFER_DESC(sizeof(QUAD_VERTS), D3D11_BIND_VERTEX_BUFFER,
		D3D11_USAGE_IMMUTABLE);
	D3D11_SUBRESOURCE_DATA srd = { QUAD_VERTS, 0, 0 };

	hr = D3D::device->CreateBuffer(&bd, &srd, &m_quad);
	CHECK(SUCCEEDED(hr), "create xfb encode quad vertex buffer");
	D3D::SetDebugObjectName(m_quad, "xfb encoder quad vertex buffer");

	// Create vertex shader

	D3DBlob* bytecode = nullptr;
	if (!D3D::CompileVertexShader(XFB_ENCODE_VS, &bytecode))
	{
		ERROR_LOG(VIDEO, "XFB encode vertex shader failed to compile");
		return;
	}

	hr = D3D::device->CreateVertexShader(bytecode->Data(), bytecode->Size(), nullptr, &m_vShader);
	CHECK(SUCCEEDED(hr), "create xfb encode vertex shader");
	D3D::SetDebugObjectName(m_vShader, "xfb encoder vertex shader");

	// Create input layout for vertex quad using bytecode from vertex shader

	hr = D3D::device->CreateInputLayout(QUAD_LAYOUT_DESC,
		sizeof(QUAD_LAYOUT_DESC)/sizeof(D3D11_INPUT_ELEMENT_DESC),
		bytecode->Data(), bytecode->Size(), &m_quadLayout);
	CHECK(SUCCEEDED(hr), "create xfb encode quad vertex layout");
	D3D::SetDebugObjectName(m_quadLayout, "xfb encoder quad layout");

	bytecode->Release();

	// Create pixel shader

	m_pShader = D3D::CompileAndCreatePixelShader(XFB_ENCODE_PS);
	if (!m_pShader)
	{
		ERROR_LOG(VIDEO, "XFB encode pixel shader failed to compile");
		return;
	}
	D3D::SetDebugObjectName(m_pShader, "xfb encoder pixel shader");

	// Create blend state

	D3D11_BLEND_DESC bld = CD3D11_BLEND_DESC(CD3D11_DEFAULT());
	hr = D3D::device->CreateBlendState(&bld, &m_xfbEncodeBlendState);
	CHECK(SUCCEEDED(hr), "create xfb encode blend state");
	D3D::SetDebugObjectName(m_xfbEncodeBlendState, "xfb encoder blend state");

	// Create depth state

	D3D11_DEPTH_STENCIL_DESC dsd = CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT());
	dsd.DepthEnable = FALSE;
	hr = D3D::device->CreateDepthStencilState(&dsd, &m_xfbEncodeDepthState);
	CHECK(SUCCEEDED(hr), "create xfb encode depth state");
	D3D::SetDebugObjectName(m_xfbEncodeDepthState, "xfb encoder depth state");

	// Create rasterizer state

	D3D11_RASTERIZER_DESC rd = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
	rd.CullMode = D3D11_CULL_NONE;
	rd.DepthClipEnable = FALSE;
	hr = D3D::device->CreateRasterizerState(&rd, &m_xfbEncodeRastState);
	CHECK(SUCCEEDED(hr), "create xfb encode rasterizer state");
	D3D::SetDebugObjectName(m_xfbEncodeRastState, "xfb encoder rast state");

	// Create EFB texture sampler

	D3D11_SAMPLER_DESC sd = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
	// FIXME: Should we really use point sampling here?
	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	hr = D3D::device->CreateSamplerState(&sd, &m_efbSampler);
	CHECK(SUCCEEDED(hr), "create xfb encode texture sampler");
	D3D::SetDebugObjectName(m_efbSampler, "xfb encoder texture sampler");
}

void XFBEncoder::Shutdown()
{
	SAFE_RELEASE(m_efbSampler);
	SAFE_RELEASE(m_xfbEncodeRastState);
	SAFE_RELEASE(m_xfbEncodeDepthState);
	SAFE_RELEASE(m_xfbEncodeBlendState);
	SAFE_RELEASE(m_pShader);
	SAFE_RELEASE(m_quadLayout);
	SAFE_RELEASE(m_vShader);
	SAFE_RELEASE(m_quad);
	SAFE_RELEASE(m_encodeParams);
	SAFE_RELEASE(m_outStage);
	SAFE_RELEASE(m_outRTV);
	SAFE_RELEASE(m_out);
}

void XFBEncoder::Encode(u8* dst, u32 width, u32 height, const EFBRectangle& srcRect, float gamma)
{
	HRESULT hr;

	// Reset API

	g_renderer->ResetAPIState();

	// Set up all the state for XFB encoding

	D3D::context->PSSetShader(m_pShader, nullptr, 0);
	D3D::context->VSSetShader(m_vShader, nullptr, 0);

	D3D::stateman->PushBlendState(m_xfbEncodeBlendState);
	D3D::stateman->PushDepthState(m_xfbEncodeDepthState);
	D3D::stateman->PushRasterizerState(m_xfbEncodeRastState);
	D3D::stateman->Apply();

	D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, FLOAT(width/2), FLOAT(height));
	D3D::context->RSSetViewports(1, &vp);

	D3D::context->IASetInputLayout(m_quadLayout);
	D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	UINT stride = sizeof(QuadVertex);
	UINT offset = 0;
	D3D::context->IASetVertexBuffers(0, 1, &m_quad, &stride, &offset);

	TargetRectangle targetRect = g_renderer->ConvertEFBRectangle(srcRect);

	XFBEncodeParams params = { 0 };
	params.Width = FLOAT(width);
	params.Height = FLOAT(height);
	params.TexLeft = FLOAT(targetRect.left) / g_renderer->GetTargetWidth();
	params.TexTop = FLOAT(targetRect.top) / g_renderer->GetTargetHeight();
	params.TexRight = FLOAT(targetRect.right) / g_renderer->GetTargetWidth();
	params.TexBottom = FLOAT(targetRect.bottom) / g_renderer->GetTargetHeight();
	params.Gamma = gamma;
	D3D::context->UpdateSubresource(m_encodeParams, 0, nullptr, &params, 0, 0);

	D3D::context->VSSetConstantBuffers(0, 1, &m_encodeParams);

	D3D::context->OMSetRenderTargets(1, &m_outRTV, nullptr);

	ID3D11ShaderResourceView* pEFB = FramebufferManager::GetEFBColorTexture()->GetSRV();

	D3D::context->PSSetConstantBuffers(0, 1, &m_encodeParams);
	D3D::context->PSSetShaderResources(0, 1, &pEFB);
	D3D::context->PSSetSamplers(0, 1, &m_efbSampler);

	// Encode!

	D3D::context->Draw(4, 0);

	// Copy to staging buffer

	D3D11_BOX srcBox = CD3D11_BOX(0, 0, 0, width/2, height, 1);
	D3D::context->CopySubresourceRegion(m_outStage, 0, 0, 0, 0, m_out, 0, &srcBox);

	// Clean up state

	IUnknown* nullDummy = nullptr;

	D3D::context->PSSetSamplers(0, 1, (ID3D11SamplerState**)&nullDummy);
	D3D::context->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&nullDummy);
	D3D::context->PSSetConstantBuffers(0, 1, (ID3D11Buffer**)&nullDummy);

	D3D::context->OMSetRenderTargets(0, nullptr, nullptr);

	D3D::context->VSSetConstantBuffers(0, 1, (ID3D11Buffer**)&nullDummy);

	D3D::stateman->PopRasterizerState();
	D3D::stateman->PopDepthState();
	D3D::stateman->PopBlendState();

	D3D::context->PSSetShader(nullptr, nullptr, 0);
	D3D::context->VSSetShader(nullptr, nullptr, 0);

	// Transfer staging buffer to GameCube/Wii RAM

	D3D11_MAPPED_SUBRESOURCE map = { 0 };
	hr = D3D::context->Map(m_outStage, 0, D3D11_MAP_READ, 0, &map);
	CHECK(SUCCEEDED(hr), "map staging buffer");

	u8* src = (u8*)map.pData;
	for (unsigned int y = 0; y < height; ++y)
	{
		memcpy(dst, src, 2*width);
		dst += bpmem.copyMipMapStrideChannels*32;
		src += map.RowPitch;
	}

	D3D::context->Unmap(m_outStage, 0);

	// Restore API

	g_renderer->RestoreAPIState();
	D3D::context->OMSetRenderTargets(1,
		&FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());
}

}
