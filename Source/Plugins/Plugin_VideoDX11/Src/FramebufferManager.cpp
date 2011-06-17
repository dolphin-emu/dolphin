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

#include "VideoConfig.h"

#include "D3DBase.h"
#include "D3DShader.h"
#include "D3DUtil.h"
#include "FramebufferManager.h"
#include "GfxState.h"
#include "HW/Memmap.h"
#include "PixelShaderCache.h"
#include "Render.h"
#include "VertexShaderCache.h"
#include "XFBEncoder.h"

namespace DX11 {

static XFBEncoder s_xfbEncoder;

D3DTexture2D* FramebufferManager::GetEFBColorTexture()
{
	FramebufferManager* pThis = (FramebufferManager*)g_framebuffer_manager;
	return pThis->m_colorTex;
}

D3DTexture2D* FramebufferManager::GetResolvedEFBColorTexture()
{
	FramebufferManager* pThis = (FramebufferManager*)g_framebuffer_manager;

	if (g_ActiveConfig.iMultisampleMode)
	{
		if (pThis->m_resolvedColorTime != pThis->m_colorTime)
		{
			D3D::context->ResolveSubresource(
				pThis->m_resolvedColorTex->GetTex(), 0,
				pThis->m_colorTex->GetTex(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
			pThis->m_resolvedColorTime = pThis->m_colorTime;
		}
		return pThis->m_resolvedColorTex;
	}
	else
	{
		pThis->m_resolvedColorTime = pThis->m_colorTime;
		return pThis->m_colorTex;
	}
}

static const char REAL_COLOR_PS[] =
"// dolphin-emu real color shader\n"

"Texture2D<float4> EFBTexture : register(t0);\n"
"sampler EFBSampler : register(s0);\n"

"struct VSOutput\n"
"{\n"
	"float4 Pos : SV_Position;\n"
	"float2 Tex : TEXCOORD;\n"
"};\n"

"void main(out float4 ocol0 : SV_Target, in VSOutput input)\n"
"{\n"
	"ocol0 = EFBTexture.Sample(EFBSampler, input.Tex);\n"
"}\n"
;

D3DTexture2D* FramebufferManager::GetRealEFBColorTexture()
{
	FramebufferManager* pThis = (FramebufferManager*)g_framebuffer_manager;

	if (pThis->m_realColorTime != pThis->m_colorTime)
	{
		if (!pThis->m_realColorShader)
		{
			pThis->m_realColorShader = D3D::CompileAndCreatePixelShader(
				REAL_COLOR_PS, sizeof(REAL_COLOR_PS));
			if (!pThis->m_realColorShader)
			{
				ERROR_LOG(VIDEO, "Failed to compile color access shader");
				return NULL;
			}
		}

		// FIXME: If EFB is multisampled, we should pick out one sample in the shader.
		D3DTexture2D* srcTex = FramebufferManager::GetResolvedEFBColorTexture();

		g_renderer->ResetAPIState();
		D3D::stateman->Apply();

		D3D::context->OMSetRenderTargets(1, &pThis->m_realColorTex->GetRTV(), NULL);

		D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, float(EFB_WIDTH), float(EFB_HEIGHT));
		D3D::context->RSSetViewports(1, &vp);

		D3D::context->PSSetShaderResources(0, 1, &srcTex->GetSRV());
		D3D::SetPointCopySampler();

		D3D::drawEncoderTexQuad(pThis->m_realColorShader);

		ID3D11ShaderResourceView* nullDummy = NULL;
		D3D::context->PSSetShaderResources(0, 1, &nullDummy);

		g_renderer->RestoreAPIState();
		D3D::context->OMSetRenderTargets(1, &pThis->m_colorTex->GetRTV(),
			pThis->m_depthTex->GetDSV());

		pThis->m_realColorTime = pThis->m_colorTime;
	}

	return pThis->m_realColorTex;
}

D3DTexture2D* FramebufferManager::GetEFBDepthTexture()
{
	FramebufferManager* pThis = (FramebufferManager*)g_framebuffer_manager;
	return pThis->m_depthTex;
}

D3DTexture2D* FramebufferManager::GetResolvedEFBDepthTexture()
{
	FramebufferManager* pThis = (FramebufferManager*)g_framebuffer_manager;

	// FIXME: Multisampled depth textures CANNOT be resolved. In DX10.1+, we
	// should use a shader to extract one sample from the depth texture. In
	// DX10.0, we can't even do that.
	// DX10.0 needs a different solution. For example, we could output depth to
	// a separate R32_FLOAT texture in the GX shaders.
	pThis->m_resolvedDepthTime = pThis->m_depthTime;
	return pThis->m_depthTex;
}

static const char REAL_DEPTH_PS[] =
"// dolphin-emu real depth shader\n"

// TODO: Support Texture2DMS depth texture (only possible in ps_4_1 or later)
"Texture2D<float> EFBTexture : register(t0);\n"
"sampler EFBSampler : register(s0);\n"

"struct VSOutput\n"
"{\n"
	"float4 Pos : SV_Position;\n"
	"float2 Tex : TEXCOORD;\n"
"};\n"

"void main(out float4 ocol0 : SV_Target, in VSOutput input)\n"
"{\n"
	"float depth = EFBTexture.Sample(EFBSampler, input.Tex);\n"
	"ocol0 = float4(depth, 0, 0, 1);\n"
"}\n"
;

D3DTexture2D* FramebufferManager::GetRealEFBDepthTexture()
{
	FramebufferManager* pThis = (FramebufferManager*)g_framebuffer_manager;

	if (pThis->m_realDepthTime != pThis->m_depthTime)
	{
		if (!pThis->m_realDepthShader)
		{
			pThis->m_realDepthShader = D3D::CompileAndCreatePixelShader(
				REAL_DEPTH_PS, sizeof(REAL_DEPTH_PS));
			if (!pThis->m_realDepthShader)
			{
				ERROR_LOG(VIDEO, "Failed to compile depth access shader");
				return NULL;
			}
		}

		D3DTexture2D* srcTex = FramebufferManager::GetResolvedEFBDepthTexture();

		g_renderer->ResetAPIState();
		D3D::stateman->Apply();

		D3D::context->OMSetRenderTargets(1, &pThis->m_realDepthTex->GetRTV(), NULL);

		D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, float(EFB_WIDTH), float(EFB_HEIGHT));
		D3D::context->RSSetViewports(1, &vp);

		D3D::context->PSSetShaderResources(0, 1, &srcTex->GetSRV());
		D3D::SetPointCopySampler();

		D3D::drawEncoderTexQuad(pThis->m_realDepthShader);

		ID3D11ShaderResourceView* nullDummy = NULL;
		D3D::context->PSSetShaderResources(0, 1, &nullDummy);

		g_renderer->RestoreAPIState();
		D3D::context->OMSetRenderTargets(1, &pThis->m_colorTex->GetRTV(),
			pThis->m_depthTex->GetDSV());

		pThis->m_realDepthTime = pThis->m_depthTime;
	}

	return pThis->m_realDepthTex;
}

D3DTexture2D* FramebufferManager::GetEFBColorTempTexture()
{
	FramebufferManager* pThis = (FramebufferManager*)g_framebuffer_manager;
	return pThis->m_colorTempTex;
}

void FramebufferManager::SwapReinterpretTexture()
{
	FramebufferManager* pThis = (FramebufferManager*)g_framebuffer_manager;
	std::swap(pThis->m_colorTempTex, pThis->m_colorTex);
}

FramebufferManager::FramebufferManager()
	: m_colorTex(NULL), m_colorTime(1),
	m_resolvedColorTex(NULL), m_resolvedColorTime(0),
	m_realColorTex(NULL), m_realColorTime(0),
	m_colorAccessStage(NULL), m_colorAccessTime(0),
	m_depthTex(NULL), m_depthTime(1),
	m_resolvedDepthTex(NULL), m_resolvedDepthTime(0),
	m_realDepthTex(NULL), m_realDepthTime(0),
	m_depthAccessStage(NULL), m_depthAccessTime(0),
	m_colorTempTex(NULL),
	m_realColorShader(NULL), m_realDepthShader(NULL)
{
	unsigned int target_width = Renderer::GetTargetWidth();
	unsigned int target_height = Renderer::GetTargetHeight();
	DXGI_SAMPLE_DESC sample_desc = D3D::GetAAMode(g_ActiveConfig.iMultisampleMode);

	ID3D11Texture2D* buf;
	D3D11_TEXTURE2D_DESC texdesc;
	HRESULT hr;

	// EFB color texture - primary render target
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, target_width, target_height, 1, 1, D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, sample_desc.Count, sample_desc.Quality);
	hr = D3D::device->CreateTexture2D(&texdesc, NULL, &buf);
	CHECK(hr==S_OK, "create EFB color texture (size: %dx%d; hr=%#x)", target_width, target_height, hr);
	m_colorTex = new D3DTexture2D(buf,
		(D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET),
		DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UNORM, (sample_desc.Count > 1));
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName(m_colorTex->GetTex(), "EFB color texture");
	D3D::SetDebugObjectName(m_colorTex->GetSRV(), "EFB color texture shader resource view");
	D3D::SetDebugObjectName(m_colorTex->GetRTV(), "EFB color texture render target view");

	// Temporary EFB color texture - used in ReinterpretPixelData
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, target_width, target_height, 1, 1, D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, 1, 0);
	hr = D3D::device->CreateTexture2D(&texdesc, NULL, &buf);
	CHECK(SUCCEEDED(hr), "create EFB color temp texture (size: %dx%d; hr=%#x)", target_width, target_height, hr);
	m_colorTempTex = new D3DTexture2D(buf,
		(D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET),
		DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UNORM);
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName(m_colorTempTex->GetTex(), "EFB color temp texture");
	D3D::SetDebugObjectName(m_colorTempTex->GetSRV(), "EFB color temp texture shader resource view");
	D3D::SetDebugObjectName(m_colorTempTex->GetRTV(), "EFB color temp texture render target view");

	// Color texture for real EFB access and texture encoder
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, EFB_WIDTH, EFB_HEIGHT, 1, 1,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
	hr = D3D::device->CreateTexture2D(&texdesc, NULL, &buf);
	CHECK(SUCCEEDED(hr), "create real EFB color texture");
	m_realColorTex = new D3DTexture2D(buf, (D3D11_BIND_FLAG)(D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET));
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName(m_realColorTex->GetTex(), "EFB real color texture");
	D3D::SetDebugObjectName(m_realColorTex->GetRTV(), "EFB real color texture rtv");
	D3D::SetDebugObjectName(m_realColorTex->GetSRV(), "EFB real color texture srv");

	// Stage for accessing real EFB color texture from CPU
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, EFB_WIDTH, EFB_HEIGHT, 1, 1,
		0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ);
	hr = D3D::device->CreateTexture2D(&texdesc, NULL, &m_colorAccessStage);
	CHECK(SUCCEEDED(hr), "create EFB color access staging buffer");
	D3D::SetDebugObjectName(m_colorAccessStage, "EFB color access stage");

	// EFB depth buffer - primary depth buffer
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R24G8_TYPELESS, target_width, target_height, 1, 1, D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, sample_desc.Count, sample_desc.Quality);
	hr = D3D::device->CreateTexture2D(&texdesc, NULL, &buf);
	CHECK(hr==S_OK, "create EFB depth texture (size: %dx%d; hr=%#x)", target_width, target_height, hr);
	m_depthTex = new D3DTexture2D(buf, (D3D11_BIND_FLAG)(D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE), DXGI_FORMAT_R24_UNORM_X8_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_UNKNOWN, (sample_desc.Count > 1));
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName(m_depthTex->GetTex(), "EFB depth texture");
	D3D::SetDebugObjectName(m_depthTex->GetDSV(), "EFB depth texture depth stencil view");
	D3D::SetDebugObjectName(m_depthTex->GetSRV(), "EFB depth texture shader resource view");

	// Depth texture for real EFB access and texture encoder
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32_FLOAT, EFB_WIDTH, EFB_HEIGHT, 1, 1,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
	hr = D3D::device->CreateTexture2D(&texdesc, NULL, &buf);
	CHECK(hr==S_OK, "create EFB depth read texture (hr=%#x)", hr);
	m_realDepthTex = new D3DTexture2D(buf,
		(D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE));
	SAFE_RELEASE(buf);
	D3D::SetDebugObjectName(m_realDepthTex->GetTex(), "EFB real depth texture");
	D3D::SetDebugObjectName(m_realDepthTex->GetRTV(), "EFB real depth texture rtv");
	D3D::SetDebugObjectName(m_realDepthTex->GetSRV(), "EFB real depth texture srv");

	// Stage for accessing real EFB depth texture from CPU
	texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32_FLOAT, EFB_WIDTH, EFB_HEIGHT, 1, 1,
		0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ);
	hr = D3D::device->CreateTexture2D(&texdesc, NULL, &m_depthAccessStage);
	CHECK(SUCCEEDED(hr), "create EFB depth access staging buffer");
	D3D::SetDebugObjectName(m_depthAccessStage, "EFB depth access stage");

	if (g_ActiveConfig.iMultisampleMode)
	{
		// Framebuffer resolve textures (color+depth)
		texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, target_width, target_height, 1, 1,
			D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, 1);
		hr = D3D::device->CreateTexture2D(&texdesc, NULL, &buf);
		CHECK(SUCCEEDED(hr), "create EFB color resolve texture (size: %dx%d)", target_width, target_height);
		m_resolvedColorTex = new D3DTexture2D(buf, D3D11_BIND_SHADER_RESOURCE, DXGI_FORMAT_R8G8B8A8_UNORM);
		SAFE_RELEASE(buf);
		D3D::SetDebugObjectName(m_resolvedColorTex->GetTex(), "EFB color resolve texture");
		D3D::SetDebugObjectName(m_resolvedColorTex->GetSRV(), "EFB color resolve texture shader resource view");

		texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R24G8_TYPELESS, target_width, target_height, 1, 1,
			D3D11_BIND_SHADER_RESOURCE);
		hr = D3D::device->CreateTexture2D(&texdesc, NULL, &buf);
		CHECK(SUCCEEDED(hr), "create EFB depth resolve texture (size: %dx%d; hr=%#x)", target_width, target_height, hr);
		m_resolvedDepthTex = new D3DTexture2D(buf, D3D11_BIND_SHADER_RESOURCE, DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
		SAFE_RELEASE(buf);
		D3D::SetDebugObjectName(m_resolvedDepthTex->GetTex(), "EFB depth resolve texture");
		D3D::SetDebugObjectName(m_resolvedDepthTex->GetSRV(), "EFB depth resolve texture shader resource view");
	}
	else
	{
		m_resolvedColorTex = NULL;
		m_resolvedDepthTex = NULL;
	}

	s_xfbEncoder.Init();
}

FramebufferManager::~FramebufferManager()
{
	SAFE_RELEASE(m_realDepthShader);
	SAFE_RELEASE(m_realColorShader);

	s_xfbEncoder.Shutdown();

	SAFE_RELEASE(m_colorTempTex);

	SAFE_RELEASE(m_depthAccessStage);
	SAFE_RELEASE(m_realDepthTex);
	SAFE_RELEASE(m_resolvedDepthTex);
	SAFE_RELEASE(m_depthTex);
	SAFE_RELEASE(m_colorAccessStage);
	SAFE_RELEASE(m_realColorTex);
	SAFE_RELEASE(m_resolvedColorTex);
	SAFE_RELEASE(m_colorTex);
}

u32 FramebufferManager::ReadColorAt(unsigned int x, unsigned int y)
{
	x = std::min(x, (unsigned int)EFB_WIDTH-1);
	y = std::min(y, (unsigned int)EFB_HEIGHT-1);

	if (m_colorAccessTime != m_colorTime)
	{
		D3DTexture2D* srcTex = FramebufferManager::GetRealEFBColorTexture();
		D3D::context->CopyResource(m_colorAccessStage, srcTex->GetTex());
		m_colorAccessTime = m_colorTime;
	}

	// TODO: Should we keep it mapped as long as the game needs it?
	D3D11_MAPPED_SUBRESOURCE map;
	HRESULT hr = D3D::context->Map(m_colorAccessStage, 0, D3D11_MAP_READ, 0, &map);
	u32 result = 0;
	if (SUCCEEDED(hr))
	{
		const u8* pData = (const u8*)map.pData;
		// FIXME: Should we byteswap the result?
		result = *(const u32*)&pData[map.RowPitch*y + 4*x];
		D3D::context->Unmap(m_colorAccessStage, 0);
	}
	else
		ERROR_LOG(VIDEO, "Failed to read from color access stage");

	return result;
}

float FramebufferManager::ReadDepthAt(unsigned int x, unsigned int y)
{
	x = std::min(x, (unsigned int)EFB_WIDTH-1);
	y = std::min(y, (unsigned int)EFB_HEIGHT-1);

	if (m_depthAccessTime != m_depthTime)
	{
		D3DTexture2D* srcTex = FramebufferManager::GetRealEFBDepthTexture();
		D3D::context->CopyResource(m_depthAccessStage, srcTex->GetTex());
		m_depthAccessTime = m_depthTime;
	}

	// TODO: Should we keep it mapped as long as the game needs it?
	D3D11_MAPPED_SUBRESOURCE map;
	HRESULT hr = D3D::context->Map(m_depthAccessStage, 0, D3D11_MAP_READ, 0, &map);
	float result = 0.f;
	if (SUCCEEDED(hr))
	{
		const u8* pData = (const u8*)map.pData;
		// FIXME: Should we byteswap the result?
		result = *(const float*)&pData[map.RowPitch*y + 4*x];
		D3D::context->Unmap(m_depthAccessStage, 0);
	}
	else
		ERROR_LOG(VIDEO, "Failed to read from depth access stage");

	return result;
}

void FramebufferManager::CopyToRealXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc,float Gamma)
{
	u8* dst = Memory::GetPointer(xfbAddr);
	s_xfbEncoder.Encode(dst, fbWidth, fbHeight, sourceRc, Gamma);
}

XFBSourceBase* FramebufferManager::CreateXFBSource(unsigned int target_width, unsigned int target_height)
{
	return new XFBSource(D3DTexture2D::Create(target_width, target_height,
		(D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE),
		D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM));
}

void FramebufferManager::GetTargetSize(unsigned int *width, unsigned int *height, const EFBRectangle& sourceRc)
{
	const float scaleX = Renderer::GetXFBScaleX();
	const float scaleY = Renderer::GetXFBScaleY();

	TargetRectangle targetSource;

	targetSource.top = (int)(sourceRc.top *scaleY);
	targetSource.bottom = (int)(sourceRc.bottom *scaleY);
	targetSource.left = (int)(sourceRc.left *scaleX);
	targetSource.right = (int)(sourceRc.right * scaleX);

	*width = targetSource.right - targetSource.left;
	*height = targetSource.bottom - targetSource.top;
}

void XFBSource::Draw(const MathUtil::Rectangle<float> &sourcerc,
	const MathUtil::Rectangle<float> &drawrc, int width, int height) const
{
	D3D::drawShadedTexSubQuad(tex->GetSRV(), &sourcerc,
		texWidth, texHeight, &drawrc, PixelShaderCache::GetColorCopyProgram(false),
		VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout());
}

void XFBSource::DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight)
{
	// DX11's XFB decoder does not use this function.
	// YUYV data is decoded in Render::Swap.
}

void XFBSource::CopyEFB(float Gamma)
{
	// Copy EFB data to XFB and restore render target again
	const D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)texWidth, (float)texHeight);

	D3D::context->RSSetViewports(1, &vp);
	D3D::context->OMSetRenderTargets(1, &tex->GetRTV(), NULL);
	D3D::SetLinearCopySampler();

	D3D::drawShadedTexQuad(FramebufferManager::GetEFBColorTexture()->GetSRV(), sourceRc.AsRECT(),
		Renderer::GetTargetWidth(), Renderer::GetTargetHeight(),
		PixelShaderCache::GetColorCopyProgram(true), VertexShaderCache::GetSimpleVertexShader(),
		VertexShaderCache::GetSimpleInputLayout(),Gamma);

	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());
}

}  // namespace DX11