// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/Memmap.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DShader.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/D3DUtil.h"
#include "VideoBackends/D3D/FramebufferManager.h"
#include "VideoBackends/D3D/PSTextureEncoder.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3D/TextureCache.h"
#include "VideoBackends/D3D/VertexShaderCache.h"

#include "VideoCommon/TextureConversionShader.h"

namespace DX11
{

struct EFBEncodeParams
{
	DWORD SrcLeft;
	DWORD SrcTop;
	DWORD DestWidth;
	DWORD ScaleFactor;
};

PSTextureEncoder::PSTextureEncoder()
	: m_ready(false), m_out(nullptr), m_outRTV(nullptr), m_outStage(nullptr),
	m_encodeParams(nullptr)
{
}

void PSTextureEncoder::Init()
{
	m_ready = false;

	HRESULT hr;

	// Create output texture RGBA format
	D3D11_TEXTURE2D_DESC t2dd = CD3D11_TEXTURE2D_DESC(
		DXGI_FORMAT_B8G8R8A8_UNORM,
		EFB_WIDTH * 4, EFB_HEIGHT / 4, 1, 1, D3D11_BIND_RENDER_TARGET);
	hr = D3D::device->CreateTexture2D(&t2dd, nullptr, &m_out);
	CHECK(SUCCEEDED(hr), "create efb encode output texture");
	D3D::SetDebugObjectName(m_out, "efb encoder output texture");

	// Create output render target view
	D3D11_RENDER_TARGET_VIEW_DESC rtvd = CD3D11_RENDER_TARGET_VIEW_DESC(m_out,
		D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_B8G8R8A8_UNORM);
	hr = D3D::device->CreateRenderTargetView(m_out, &rtvd, &m_outRTV);
	CHECK(SUCCEEDED(hr), "create efb encode output render target view");
	D3D::SetDebugObjectName(m_outRTV, "efb encoder output rtv");

	// Create output staging buffer
	t2dd.Usage = D3D11_USAGE_STAGING;
	t2dd.BindFlags = 0;
	t2dd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	hr = D3D::device->CreateTexture2D(&t2dd, nullptr, &m_outStage);
	CHECK(SUCCEEDED(hr), "create efb encode output staging buffer");
	D3D::SetDebugObjectName(m_outStage, "efb encoder output staging buffer");

	// Create constant buffer for uploading data to shaders
	D3D11_BUFFER_DESC bd = CD3D11_BUFFER_DESC(sizeof(EFBEncodeParams),
		D3D11_BIND_CONSTANT_BUFFER);
	hr = D3D::device->CreateBuffer(&bd, nullptr, &m_encodeParams);
	CHECK(SUCCEEDED(hr), "create efb encode params buffer");
	D3D::SetDebugObjectName(m_encodeParams, "efb encoder params buffer");

	m_ready = true;
}

void PSTextureEncoder::Shutdown()
{
	m_ready = false;

	for (auto& it : m_staticShaders)
	{
		SAFE_RELEASE(it.second);
	}
	m_staticShaders.clear();

	SAFE_RELEASE(m_encodeParams);
	SAFE_RELEASE(m_outStage);
	SAFE_RELEASE(m_outRTV);
	SAFE_RELEASE(m_out);
}

void PSTextureEncoder::Encode(u8* dst, const TextureCache::TCacheEntryBase *texture_entry,
	PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
	bool isIntensity, bool scaleByHalf)
{
	if (!m_ready) // Make sure we initialized OK
		return;

	HRESULT hr;

	// Reset API
	g_renderer->ResetAPIState();

	// Set up all the state for EFB encoding
	{
		D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, FLOAT(texture_entry->CacheLinesPerRow() * 8), FLOAT(texture_entry->NumBlocksY()));
		D3D::context->RSSetViewports(1, &vp);

		constexpr EFBRectangle fullSrcRect(0, 0, EFB_WIDTH, EFB_HEIGHT);
		TargetRectangle targetRect = g_renderer->ConvertEFBRectangle(fullSrcRect);

		D3D::context->OMSetRenderTargets(1, &m_outRTV, nullptr);

		ID3D11ShaderResourceView* pEFB = (srcFormat == PEControl::Z24) ?
			FramebufferManager::GetResolvedEFBDepthTexture()->GetSRV() :
			// FIXME: Instead of resolving EFB, it would be better to pick out a
			// single sample from each pixel. The game may break if it isn't
			// expecting the blurred edges around multisampled shapes.
			FramebufferManager::GetResolvedEFBColorTexture()->GetSRV();

		EFBEncodeParams params;
		params.SrcLeft = srcRect.left;
		params.SrcTop = srcRect.top;
		params.DestWidth = texture_entry->native_width;
		params.ScaleFactor = scaleByHalf ? 2 : 1;
		D3D::context->UpdateSubresource(m_encodeParams, 0, nullptr, &params, 0, 0);
		D3D::stateman->SetPixelConstants(m_encodeParams);

		// Use linear filtering if (bScaleByHalf), use point filtering otherwise
		if (scaleByHalf)
			D3D::SetLinearCopySampler();
		else
			D3D::SetPointCopySampler();

		D3D::drawShadedTexQuad(pEFB,
			targetRect.AsRECT(),
			Renderer::GetTargetWidth(),
			Renderer::GetTargetHeight(),
			SetStaticShader(texture_entry->format, srcFormat, isIntensity, scaleByHalf),
			VertexShaderCache::GetSimpleVertexShader(),
			VertexShaderCache::GetSimpleInputLayout());

		// Copy to staging buffer
		D3D11_BOX srcBox = CD3D11_BOX(0, 0, 0, texture_entry->CacheLinesPerRow() * 8, texture_entry->NumBlocksY(), 1);
		D3D::context->CopySubresourceRegion(m_outStage, 0, 0, 0, 0, m_out, 0, &srcBox);

		// Transfer staging buffer to GameCube/Wii RAM
		D3D11_MAPPED_SUBRESOURCE map = { 0 };
		hr = D3D::context->Map(m_outStage, 0, D3D11_MAP_READ, 0, &map);
		CHECK(SUCCEEDED(hr), "map staging buffer (0x%x)", hr);

		u8* src = (u8*)map.pData;
		u32 readStride = std::min(texture_entry->CacheLinesPerRow() * 32, map.RowPitch);
		for (unsigned int y = 0; y < texture_entry->NumBlocksY(); ++y)
		{
			memcpy(dst, src, readStride);
			dst += texture_entry->memory_stride;
			src += map.RowPitch;
		}

		D3D::context->Unmap(m_outStage, 0);
	}

	// Restore API
	g_renderer->RestoreAPIState();
	D3D::context->OMSetRenderTargets(1,
		&FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());
}

ID3D11PixelShader* PSTextureEncoder::SetStaticShader(unsigned int dstFormat, PEControl::PixelFormat srcFormat,
	bool isIntensity, bool scaleByHalf)
{
	size_t fetchNum = static_cast<size_t>(srcFormat);
	size_t scaledFetchNum = scaleByHalf ? 1 : 0;
	size_t intensityNum = isIntensity ? 1 : 0;
	size_t generatorNum = dstFormat;

	ComboKey key = MakeComboKey(dstFormat, srcFormat, isIntensity, scaleByHalf);

	ComboMap::iterator it = m_staticShaders.find(key);
	if (it == m_staticShaders.end())
	{
		INFO_LOG(VIDEO, "Compiling efb encoding shader for dstFormat 0x%X, srcFormat %d, isIntensity %d, scaleByHalf %d",
			dstFormat, static_cast<int>(srcFormat), isIntensity ? 1 : 0, scaleByHalf ? 1 : 0);

		u32 format = dstFormat;

		if (srcFormat == PEControl::Z24)
		{
			format |= _GX_TF_ZTF;
			if (dstFormat == 11)
				format = GX_TF_Z16;
			else if (format < GX_TF_Z8 || format > GX_TF_Z24X8)
				format |= _GX_TF_CTF;
		}
		else
		{
			if (dstFormat > GX_TF_RGBA8 || (dstFormat < GX_TF_RGB565 && !isIntensity))
				format |= _GX_TF_CTF;
		}

		D3DBlob* bytecode = nullptr;
		const char* shader = TextureConversionShader::GenerateEncodingShader(format, API_D3D);
		if (!D3D::CompilePixelShader(shader, &bytecode))
		{
			WARN_LOG(VIDEO, "EFB encoder shader for dstFormat 0x%X, srcFormat %d, isIntensity %d, scaleByHalf %d failed to compile",
				dstFormat, static_cast<int>(srcFormat), isIntensity ? 1 : 0, scaleByHalf ? 1 : 0);
			m_staticShaders[key] = nullptr;
			return nullptr;
		}

		ID3D11PixelShader* newShader;
		HRESULT hr = D3D::device->CreatePixelShader(bytecode->Data(), bytecode->Size(), nullptr, &newShader);
		CHECK(SUCCEEDED(hr), "create efb encoder pixel shader");

		char debugName[255] = {};
		sprintf_s(debugName, "efb encoder pixel shader (dst:%d, src:%d, intensity:%d, scale:%d)",
			dstFormat, srcFormat, isIntensity, scaleByHalf);
		D3D::SetDebugObjectName(newShader, debugName);

		it = m_staticShaders.emplace(key, newShader).first;
		bytecode->Release();
	}

	return it->second;
}

}
