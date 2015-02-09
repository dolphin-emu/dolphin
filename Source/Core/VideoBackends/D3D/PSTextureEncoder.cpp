// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
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

size_t PSTextureEncoder::Encode(u8* dst, unsigned int dstFormat,
	PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
	bool isIntensity, bool scaleByHalf)
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

	unsigned int blockW = BLOCK_WIDTHS[dstFormat];
	unsigned int blockH = BLOCK_HEIGHTS[dstFormat];

	// Round up source dims to multiple of block size
	unsigned int actualWidth = correctSrc.GetWidth() / (scaleByHalf ? 2 : 1);
	actualWidth = (actualWidth + blockW-1) & ~(blockW-1);
	unsigned int actualHeight = correctSrc.GetHeight() / (scaleByHalf ? 2 : 1);
	actualHeight = (actualHeight + blockH-1) & ~(blockH-1);

	unsigned int numBlocksX = actualWidth/blockW;
	unsigned int numBlocksY = actualHeight/blockH;

	unsigned int cacheLinesPerRow;
	if (dstFormat == 0x6) // RGBA takes two cache lines per block; all others take one
		cacheLinesPerRow = numBlocksX*2;
	else
		cacheLinesPerRow = numBlocksX;
	_assert_msg_(VIDEO, cacheLinesPerRow*32 <= MAX_BYTES_PER_BLOCK_ROW, "cache lines per row sanity check");

	unsigned int totalCacheLines = cacheLinesPerRow * numBlocksY;
	_assert_msg_(VIDEO, totalCacheLines*32 <= MAX_BYTES_PER_ENCODE, "total encode size sanity check");

	size_t encodeSize = 0;

	// Reset API
	g_renderer->ResetAPIState();

	// Set up all the state for EFB encoding

	{
		D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, FLOAT(cacheLinesPerRow * 8), FLOAT(numBlocksY));
		D3D::context->RSSetViewports(1, &vp);

		EFBRectangle fullSrcRect;
		fullSrcRect.left = 0;
		fullSrcRect.top = 0;
		fullSrcRect.right = EFB_WIDTH;
		fullSrcRect.bottom = EFB_HEIGHT;
		TargetRectangle targetRect = g_renderer->ConvertEFBRectangle(fullSrcRect);

		D3D::context->OMSetRenderTargets(1, &m_outRTV, nullptr);

		ID3D11ShaderResourceView* pEFB = (srcFormat == PEControl::Z24) ?
			FramebufferManager::GetResolvedEFBDepthTexture()->GetSRV() :
			// FIXME: Instead of resolving EFB, it would be better to pick out a
			// single sample from each pixel. The game may break if it isn't
			// expecting the blurred edges around multisampled shapes.
			FramebufferManager::GetResolvedEFBColorTexture()->GetSRV();

		EFBEncodeParams params;
		params.SrcLeft = correctSrc.left;
		params.SrcTop = correctSrc.top;
		params.DestWidth = actualWidth;
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
			SetStaticShader(dstFormat, srcFormat, isIntensity, scaleByHalf),
			VertexShaderCache::GetSimpleVertexShader(),
			VertexShaderCache::GetSimpleInputLayout());

		// Copy to staging buffer
		D3D11_BOX srcBox = CD3D11_BOX(0, 0, 0, cacheLinesPerRow * 8, numBlocksY, 1);
		D3D::context->CopySubresourceRegion(m_outStage, 0, 0, 0, 0, m_out, 0, &srcBox);

		// Transfer staging buffer to GameCube/Wii RAM
		D3D11_MAPPED_SUBRESOURCE map = { 0 };
		hr = D3D::context->Map(m_outStage, 0, D3D11_MAP_READ, 0, &map);
		CHECK(SUCCEEDED(hr), "map staging buffer (0x%x)", hr);

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

		it = m_staticShaders.insert(std::make_pair(key, newShader)).first;
		bytecode->Release();
	}

	return it->second;
}

}
