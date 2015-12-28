// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DShader.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/D3DUtil.h"
#include "VideoBackends/D3D/FramebufferManager.h"
#include "VideoBackends/D3D/GeometryShaderCache.h"
#include "VideoBackends/D3D/PixelShaderCache.h"
#include "VideoBackends/D3D/PSTextureEncoder.h"
#include "VideoBackends/D3D/TextureCache.h"
#include "VideoBackends/D3D/TextureEncoder.h"
#include "VideoBackends/D3D/VertexShaderCache.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{

static std::unique_ptr<TextureEncoder> g_encoder;
const size_t MAX_COPY_BUFFERS = 32;
ID3D11Buffer* efbcopycbuf[MAX_COPY_BUFFERS] = { 0 };

TextureCache::TCacheEntry::~TCacheEntry()
{
	texture->Release();
}

void TextureCache::TCacheEntry::Bind(unsigned int stage)
{
	D3D::stateman->SetTexture(stage, texture->GetSRV());
}

bool TextureCache::TCacheEntry::Save(const std::string& filename, unsigned int level)
{
	// TODO: Somehow implement this (D3DX11 doesn't support dumping individual LODs)
	static bool warn_once = true;
	if (level && warn_once)
	{
		WARN_LOG(VIDEO, "Dumping individual LOD not supported by D3D11 backend!");
		warn_once = false;
		return false;
	}

	ID3D11Texture2D* pNewTexture = nullptr;
	ID3D11Texture2D* pSurface = texture->GetTex();
	D3D11_TEXTURE2D_DESC desc;
	pSurface->GetDesc(&desc);

	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	desc.Usage = D3D11_USAGE_STAGING;

	HRESULT hr = D3D::device->CreateTexture2D(&desc, nullptr, &pNewTexture);

	bool saved_png = false;

	if (SUCCEEDED(hr) && pNewTexture)
	{
		D3D::context->CopyResource(pNewTexture, pSurface);

		D3D11_MAPPED_SUBRESOURCE map;
		hr = D3D::context->Map(pNewTexture, 0, D3D11_MAP_READ_WRITE, 0, &map);
		if (SUCCEEDED(hr))
		{
			saved_png = TextureToPng((u8*)map.pData, map.RowPitch, filename, desc.Width, desc.Height);
			D3D::context->Unmap(pNewTexture, 0);
		}
		SAFE_RELEASE(pNewTexture);
	}

	return saved_png;
}

void TextureCache::TCacheEntry::CopyRectangleFromTexture(
	const TCacheEntryBase* source,
	const MathUtil::Rectangle<int> &srcrect,
	const MathUtil::Rectangle<int> &dstrect)
{
	TCacheEntry* srcentry = (TCacheEntry*)source;
	if (srcrect.GetWidth() == dstrect.GetWidth()
		&& srcrect.GetHeight() == dstrect.GetHeight())
	{
		const D3D11_BOX *psrcbox = nullptr;
		D3D11_BOX srcbox;
		if (srcrect.left != 0 || srcrect.top != 0)
		{
			srcbox.left = srcrect.left;
			srcbox.top = srcrect.top;
			srcbox.right = srcrect.right;
			srcbox.bottom = srcrect.bottom;
			psrcbox = &srcbox;
		}
		D3D::context->CopySubresourceRegion(
			texture->GetTex(),
			0,
			dstrect.left,
			dstrect.top,
			0,
			srcentry->texture->GetTex(),
			0,
			psrcbox);
		return;
	}
	else if (!config.rendertarget)
	{
		return;
	}
	g_renderer->ResetAPIState(); // reset any game specific settings

	const D3D11_VIEWPORT vp = CD3D11_VIEWPORT(
		float(dstrect.left),
		float(dstrect.top),
		float(dstrect.GetWidth()),
		float(dstrect.GetHeight()));

	D3D::context->OMSetRenderTargets(1, &texture->GetRTV(), nullptr);
	D3D::context->RSSetViewports(1, &vp);
	D3D::SetLinearCopySampler();
	D3D11_RECT srcRC;
	srcRC.left = srcrect.left;
	srcRC.right = srcrect.right;
	srcRC.top = srcrect.top;
	srcRC.bottom = srcrect.bottom;
	D3D::drawShadedTexQuad(srcentry->texture->GetSRV(), &srcRC,
		srcentry->config.width, srcentry->config.height,
		PixelShaderCache::GetColorCopyProgram(false),
		VertexShaderCache::GetSimpleVertexShader(),
		VertexShaderCache::GetSimpleInputLayout(), nullptr, 1.0, 0);

	D3D::context->OMSetRenderTargets(1,
		&FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());

	g_renderer->RestoreAPIState();
}

void TextureCache::TCacheEntry::Load(unsigned int width, unsigned int height,
	unsigned int expanded_width, unsigned int level)
{
	unsigned int src_pitch = 4 * expanded_width;
	D3D::ReplaceRGBATexture2D(texture->GetTex(), TextureCache::temp, width, height, src_pitch, level, usage);
}

TextureCacheBase::TCacheEntryBase* TextureCache::CreateTexture(const TCacheEntryConfig& config)
{
	if (config.rendertarget)
	{
		return new TCacheEntry(config, D3DTexture2D::Create(config.width, config.height,
			(D3D11_BIND_FLAG)((int)D3D11_BIND_RENDER_TARGET | (int)D3D11_BIND_SHADER_RESOURCE),
			D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM, 1, config.layers));
	}
	else
	{
		D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
		D3D11_CPU_ACCESS_FLAG cpu_access = (D3D11_CPU_ACCESS_FLAG)0;

		if (config.levels == 1)
		{
			usage = D3D11_USAGE_DYNAMIC;
			cpu_access = D3D11_CPU_ACCESS_WRITE;
		}

		const D3D11_TEXTURE2D_DESC texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM,
			config.width, config.height, 1, config.levels, D3D11_BIND_SHADER_RESOURCE, usage, cpu_access);

		ID3D11Texture2D *pTexture;
		const HRESULT hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &pTexture);
		CHECK(SUCCEEDED(hr), "Create texture of the TextureCache");

		TCacheEntry* const entry = new TCacheEntry(config, new D3DTexture2D(pTexture, D3D11_BIND_SHADER_RESOURCE));
		entry->usage = usage;

		// TODO: better debug names
		D3D::SetDebugObjectName((ID3D11DeviceChild*)entry->texture->GetTex(), "a texture of the TextureCache");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)entry->texture->GetSRV(), "shader resource view of a texture of the TextureCache");

		SAFE_RELEASE(pTexture);

		return entry;
	}
}

void TextureCache::TCacheEntry::FromRenderTarget(u8* dst, PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
	bool scaleByHalf, unsigned int cbufid, const float *colmat)
{
	// When copying at half size, in multisampled mode, resolve the color/depth buffer first.
	// This is because multisampled texture reads go through Load, not Sample, and the linear
	// filter is ignored.
	bool multisampled = (g_ActiveConfig.iMultisamples > 1);
	ID3D11ShaderResourceView* efbTexSRV = (srcFormat == PEControl::Z24) ?
		FramebufferManager::GetEFBDepthTexture()->GetSRV() :
		FramebufferManager::GetEFBColorTexture()->GetSRV();
	if (multisampled && scaleByHalf)
	{
		multisampled = false;
		efbTexSRV = (srcFormat == PEControl::Z24) ?
			FramebufferManager::GetResolvedEFBDepthTexture()->GetSRV() :
			FramebufferManager::GetResolvedEFBColorTexture()->GetSRV();
	}

	g_renderer->ResetAPIState();

	// stretch picture with increased internal resolution
	const D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)config.width, (float)config.height);
	D3D::context->RSSetViewports(1, &vp);

	// set transformation
	if (nullptr == efbcopycbuf[cbufid])
	{
		const D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(28 * sizeof(float), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DEFAULT);
		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = colmat;
		HRESULT hr = D3D::device->CreateBuffer(&cbdesc, &data, &efbcopycbuf[cbufid]);
		CHECK(SUCCEEDED(hr), "Create efb copy constant buffer %d", cbufid);
		D3D::SetDebugObjectName((ID3D11DeviceChild*)efbcopycbuf[cbufid], "a constant buffer used in TextureCache::CopyRenderTargetToTexture");
	}
	D3D::stateman->SetPixelConstants(efbcopycbuf[cbufid]);

	const TargetRectangle targetSource = g_renderer->ConvertEFBRectangle(srcRect);
	// TODO: try targetSource.asRECT();
	const D3D11_RECT sourcerect = CD3D11_RECT(targetSource.left, targetSource.top, targetSource.right, targetSource.bottom);

	// Use linear filtering if (bScaleByHalf), use point filtering otherwise
	if (scaleByHalf)
		D3D::SetLinearCopySampler();
	else
		D3D::SetPointCopySampler();

	// Make sure we don't draw with the texture set as both a source and target.
	// (This can happen because we don't unbind textures when we free them.)
	D3D::stateman->UnsetTexture(texture->GetSRV());

	D3D::context->OMSetRenderTargets(1, &texture->GetRTV(), nullptr);

	// Create texture copy
	D3D::drawShadedTexQuad(
		efbTexSRV,
		&sourcerect, Renderer::GetTargetWidth(),
		Renderer::GetTargetHeight(),
		srcFormat == PEControl::Z24 ? PixelShaderCache::GetDepthMatrixProgram(multisampled) : PixelShaderCache::GetColorMatrixProgram(multisampled),
		VertexShaderCache::GetSimpleVertexShader(),
		VertexShaderCache::GetSimpleInputLayout(),
		GeometryShaderCache::GetCopyGeometryShader());

	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());

	g_renderer->RestoreAPIState();
}

void TextureCache::CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
	PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
	bool isIntensity, bool scaleByHalf)
{
	g_encoder->Encode(dst, format, native_width, bytes_per_row, num_blocks_y, memory_stride, srcFormat, srcRect, isIntensity, scaleByHalf);
}

const char palette_shader[] =
R"HLSL(
sampler samp0 : register(s0);
Texture2DArray Tex0 : register(t0);
Buffer<uint> Tex1 : register(t1);
uniform float Multiply;

uint Convert3To8(uint v)
{
	// Swizzle bits: 00000123 -> 12312312
	return (v << 5) | (v << 2) | (v >> 1);
}

uint Convert4To8(uint v)
{
	// Swizzle bits: 00001234 -> 12341234
	return (v << 4) | v;
}

uint Convert5To8(uint v)
{
	// Swizzle bits: 00012345 -> 12345123
	return (v << 3) | (v >> 2);
}

uint Convert6To8(uint v)
{
	// Swizzle bits: 00123456 -> 12345612
	return (v << 2) | (v >> 4);
}

float4 DecodePixel_RGB5A3(uint val)
{
	int r,g,b,a;
	if ((val&0x8000))
	{
		r=Convert5To8((val>>10) & 0x1f);
		g=Convert5To8((val>>5 ) & 0x1f);
		b=Convert5To8((val    ) & 0x1f);
		a=0xFF;
	}
	else
	{
		a=Convert3To8((val>>12) & 0x7);
		r=Convert4To8((val>>8 ) & 0xf);
		g=Convert4To8((val>>4 ) & 0xf);
		b=Convert4To8((val    ) & 0xf);
	}
	return float4(r, g, b, a) / 255;
}

float4 DecodePixel_RGB565(uint val)
{
	int r, g, b, a;
	r = Convert5To8((val >> 11) & 0x1f);
	g = Convert6To8((val >> 5) & 0x3f);
	b = Convert5To8((val) & 0x1f);
	a = 0xFF;
	return float4(r, g, b, a) / 255;
}

float4 DecodePixel_IA8(uint val)
{
	int i = val & 0xFF;
	int a = val >> 8;
	return float4(i, i, i, a) / 255;
}

void main(
	out float4 ocol0 : SV_Target,
	in float4 pos : SV_Position,
	in float3 uv0 : TEXCOORD0)
{
	uint src = round(Tex0.Sample(samp0,uv0) * Multiply).r;
	src = Tex1.Load(src);
	src = ((src << 8) & 0xFF00) | (src >> 8);
	ocol0 = DECODE(src);
}
)HLSL";

void TextureCache::ConvertTexture(TCacheEntryBase* entry, TCacheEntryBase* unconverted, void* palette, TlutFormat format)
{
	g_renderer->ResetAPIState();

	// stretch picture with increased internal resolution
	const D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)unconverted->config.width, (float)unconverted->config.height);
	D3D::context->RSSetViewports(1, &vp);

	D3D11_BOX box{ 0, 0, 0, 512, 1, 1 };
	D3D::context->UpdateSubresource(palette_buf, 0, &box, palette, 0, 0);

	D3D::stateman->SetTexture(1, palette_buf_srv);

	// TODO: Add support for C14X2 format.  (Different multiplier, more palette entries.)
	float params[4] = { (unconverted->format & 0xf) == 0 ? 15.f : 255.f };
	D3D::context->UpdateSubresource(palette_uniform, 0, nullptr, &params, 0, 0);
	D3D::stateman->SetPixelConstants(palette_uniform);

	const D3D11_RECT sourcerect = CD3D11_RECT(0, 0, unconverted->config.width, unconverted->config.height);

	D3D::SetPointCopySampler();

	// Make sure we don't draw with the texture set as both a source and target.
	// (This can happen because we don't unbind textures when we free them.)
	D3D::stateman->UnsetTexture(static_cast<TCacheEntry*>(entry)->texture->GetSRV());

	D3D::context->OMSetRenderTargets(1, &static_cast<TCacheEntry*>(entry)->texture->GetRTV(), nullptr);

	// Create texture copy
	D3D::drawShadedTexQuad(
		static_cast<TCacheEntry*>(unconverted)->texture->GetSRV(),
		&sourcerect, unconverted->config.width, unconverted->config.height,
		palette_pixel_shader[format],
		VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(),
		GeometryShaderCache::GetCopyGeometryShader());

	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(), FramebufferManager::GetEFBDepthTexture()->GetDSV());

	g_renderer->RestoreAPIState();
}

ID3D11PixelShader *GetConvertShader(const char* Type)
{
	std::string shader = "#define DECODE DecodePixel_";
	shader.append(Type);
	shader.append("\n");
	shader.append(palette_shader);
	return D3D::CompileAndCreatePixelShader(shader);
}

TextureCache::TextureCache()
{
	// FIXME: Is it safe here?
	g_encoder = std::make_unique<PSTextureEncoder>();
	g_encoder->Init();

	palette_buf = nullptr;
	palette_buf_srv = nullptr;
	palette_uniform = nullptr;
	palette_pixel_shader[GX_TL_IA8] = GetConvertShader("IA8");
	palette_pixel_shader[GX_TL_RGB565] = GetConvertShader("RGB565");
	palette_pixel_shader[GX_TL_RGB5A3] = GetConvertShader("RGB5A3");
	auto lutBd = CD3D11_BUFFER_DESC(sizeof(u16) * 256, D3D11_BIND_SHADER_RESOURCE);
	HRESULT hr = D3D::device->CreateBuffer(&lutBd, nullptr, &palette_buf);
	CHECK(SUCCEEDED(hr), "create palette decoder lut buffer");
	D3D::SetDebugObjectName(palette_buf, "texture decoder lut buffer");
	// TODO: C14X2 format.
	auto outlutUavDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(palette_buf, DXGI_FORMAT_R16_UINT, 0, 256, 0);
	hr = D3D::device->CreateShaderResourceView(palette_buf, &outlutUavDesc, &palette_buf_srv);
	CHECK(SUCCEEDED(hr), "create palette decoder lut srv");
	D3D::SetDebugObjectName(palette_buf_srv, "texture decoder lut srv");
	const D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(16, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DEFAULT);
	hr = D3D::device->CreateBuffer(&cbdesc, nullptr, &palette_uniform);
	CHECK(SUCCEEDED(hr), "Create palette decoder constant buffer");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)palette_uniform, "a constant buffer used in TextureCache::CopyRenderTargetToTexture");
}

TextureCache::~TextureCache()
{
	for (unsigned int k = 0; k < MAX_COPY_BUFFERS; ++k)
		SAFE_RELEASE(efbcopycbuf[k]);

	g_encoder->Shutdown();
	g_encoder.reset();

	SAFE_RELEASE(palette_buf);
	SAFE_RELEASE(palette_buf_srv);
	SAFE_RELEASE(palette_uniform);
	for (ID3D11PixelShader*& shader : palette_pixel_shader)
		SAFE_RELEASE(shader);
}

}
