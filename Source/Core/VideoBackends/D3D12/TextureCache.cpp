// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DShader.h"
#include "VideoBackends/D3D12/D3DState.h"
#include "VideoBackends/D3D12/D3DStreamBuffer.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/PSTextureEncoder.h"
#include "VideoBackends/D3D12/StaticShaderCache.h"
#include "VideoBackends/D3D12/TextureCache.h"
#include "VideoBackends/D3D12/TextureEncoder.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{

static std::unique_ptr<TextureEncoder> s_encoder = nullptr;

static std::unique_ptr<D3DStreamBuffer> s_efb_copy_stream_buffer = nullptr;

static ID3D12Resource* s_texture_cache_entry_readback_buffer = nullptr;
static void* s_texture_cache_entry_readback_buffer_data = nullptr;
static UINT s_texture_cache_entry_readback_buffer_size = 0;

TextureCache::TCacheEntry::~TCacheEntry()
{
	m_texture->Release();
}

void TextureCache::TCacheEntry::Bind(unsigned int stage)
{
	// Textures bound as group in TextureCache::BindTextures.
}

bool TextureCache::TCacheEntry::Save(const std::string& filename, unsigned int level)
{
	// EXISTINGD3D11TODO: Somehow implement this (D3DX11 doesn't support dumping individual LODs)
	static bool warn_once = true;
	if (level && warn_once)
	{
		WARN_LOG(VIDEO, "Dumping individual LOD not supported by D3D12 backend!");
		warn_once = false;
		return false;
	}

	D3D12_RESOURCE_DESC texture_desc = m_texture->GetTex12()->GetDesc();

	const unsigned int required_readback_buffer_size = D3D::AlignValue(static_cast<unsigned int>(texture_desc.Width) * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	if (s_texture_cache_entry_readback_buffer_size < required_readback_buffer_size)
	{
		s_texture_cache_entry_readback_buffer_size = required_readback_buffer_size;

		// We know the readback buffer won't be in use right now, since we wait on this thread
		// for the GPU to finish execution right after copying to it.

		SAFE_RELEASE(s_texture_cache_entry_readback_buffer);
	}

	if (!s_texture_cache_entry_readback_buffer_size)
	{
		CheckHR(
			D3D::device12->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(s_texture_cache_entry_readback_buffer_size),
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&s_texture_cache_entry_readback_buffer)
				)
			);

		CheckHR(s_texture_cache_entry_readback_buffer->Map(0, nullptr, &s_texture_cache_entry_readback_buffer_data));
	}

	bool saved_png = false;

	m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);

	D3D12_TEXTURE_COPY_LOCATION dst_location = {};
	dst_location.pResource = s_texture_cache_entry_readback_buffer;
	dst_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst_location.PlacedFootprint.Offset = 0;
	dst_location.PlacedFootprint.Footprint.Depth = 1;
	dst_location.PlacedFootprint.Footprint.Format = texture_desc.Format;
	dst_location.PlacedFootprint.Footprint.Width = static_cast<UINT>(texture_desc.Width);
	dst_location.PlacedFootprint.Footprint.Height = texture_desc.Height;
	dst_location.PlacedFootprint.Footprint.RowPitch = D3D::AlignValue(dst_location.PlacedFootprint.Footprint.Width * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	D3D12_TEXTURE_COPY_LOCATION src_location = CD3DX12_TEXTURE_COPY_LOCATION(m_texture->GetTex12(), 0);

	D3D::current_command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);

	D3D::command_list_mgr->ExecuteQueuedWork(true);

	saved_png = TextureToPng(
		static_cast<u8*>(s_texture_cache_entry_readback_buffer_data),
		dst_location.PlacedFootprint.Footprint.RowPitch,
		filename,
		dst_location.PlacedFootprint.Footprint.Width,
		dst_location.PlacedFootprint.Footprint.Height
		);

	return saved_png;
}

void TextureCache::TCacheEntry::CopyRectangleFromTexture(
	const TCacheEntryBase* source,
	const MathUtil::Rectangle<int>& src_rect,
	const MathUtil::Rectangle<int>& dst_rect)
{
	const TCacheEntry* srcentry = reinterpret_cast<const TCacheEntry*>(source);
	if (src_rect.GetWidth() == dst_rect.GetWidth()
		&& src_rect.GetHeight() == dst_rect.GetHeight())
	{
		const D3D12_BOX* src_box_pointer = nullptr;
		D3D12_BOX src_box;
		if (src_rect.left != 0 || src_rect.top != 0)
		{
			src_box.front = 0;
			src_box.back = 1;
			src_box.left = src_rect.left;
			src_box.top = src_rect.top;
			src_box.right = src_rect.right;
			src_box.bottom = src_rect.bottom;
			src_box_pointer = &src_box;
		}

		if (static_cast<u32>(src_rect.GetHeight()) > config.height ||
			static_cast<u32>(src_rect.GetWidth()) > config.width)
		{
			// To mimic D3D11 behavior, we're just going to drop the clear since it is invalid.
			// This invalid copy needs to be fixed above the Backend level.

			// On D3D12, instead of silently dropping this invalid clear, the runtime throws an exception
			// so we need to filter it out ourselves.

			return;
		}

		D3D12_TEXTURE_COPY_LOCATION dst_location = CD3DX12_TEXTURE_COPY_LOCATION(m_texture->GetTex12(), 0);
		D3D12_TEXTURE_COPY_LOCATION src_location = CD3DX12_TEXTURE_COPY_LOCATION(srcentry->m_texture->GetTex12(), 0);

		m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_DEST);
		srcentry->m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);

		D3D::current_command_list->CopyTextureRegion(&dst_location, dst_rect.left, dst_rect.top, 0, &src_location, src_box_pointer);

		m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		srcentry->m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		return;
	}
	else if (!config.rendertarget)
	{
		return;
	}

	const D3D12_VIEWPORT vp = {
		float(dst_rect.left),
		float(dst_rect.top),
		float(dst_rect.GetWidth()),
		float(dst_rect.GetHeight()),
		D3D12_MIN_DEPTH,
		D3D12_MAX_DEPTH
	};
	D3D::current_command_list->RSSetViewports(1, &vp);

	m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(1, &m_texture->GetRTV12(), FALSE, nullptr);

	D3D::SetLinearCopySampler();

	D3D12_RECT src_rc;
	src_rc.left = src_rect.left;
	src_rc.right = src_rect.right;
	src_rc.top = src_rect.top;
	src_rc.bottom = src_rect.bottom;

	D3D::DrawShadedTexQuad(srcentry->m_texture, &src_rc,
		srcentry->config.width, srcentry->config.height,
		StaticShaderCache::GetColorCopyPixelShader(false),
		StaticShaderCache::GetSimpleVertexShader(),
		StaticShaderCache::GetSimpleVertexShaderInputLayout(), D3D12_SHADER_BYTECODE(), 1.0, 0,
		DXGI_FORMAT_R8G8B8A8_UNORM, false, m_texture->GetMultisampled());

	m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());

	g_renderer->RestoreAPIState();
}

void TextureCache::TCacheEntry::Load(unsigned int width, unsigned int height,
	unsigned int expanded_width, unsigned int level)
{
	unsigned int src_pitch = 4 * expanded_width;
	D3D::ReplaceRGBATexture2D(m_texture->GetTex12(), TextureCache::temp, width, height, src_pitch, level, m_texture->GetResourceUsageState());
}

TextureCacheBase::TCacheEntryBase* TextureCache::CreateTexture(const TCacheEntryConfig& config)
{
	if (config.rendertarget)
	{
		D3DTexture2D* texture = D3DTexture2D::Create(config.width, config.height,
			static_cast<D3D11_BIND_FLAG>((static_cast<int>(D3D11_BIND_RENDER_TARGET) | static_cast<int>(D3D11_BIND_SHADER_RESOURCE))),
			D3D11_USAGE_DEFAULT, DXGI_FORMAT_R8G8B8A8_UNORM, 1, config.layers);

		TCacheEntry* entry = new TCacheEntry(config, texture);

		entry->m_texture_srv_cpu_handle = texture->GetSRV12CPU();
		entry->m_texture_srv_gpu_handle = texture->GetSRV12GPU();
		entry->m_texture_srv_gpu_handle_cpu_shadow = texture->GetSRV12GPUCPUShadow();

		return entry;
	}
	else
	{
		ID3D12Resource* texture_resource = nullptr;

		D3D12_RESOURCE_DESC texture_resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM,
			config.width, config.height, 1, config.levels);

		CheckHR(
			D3D::device12->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC(texture_resource_desc),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS(&texture_resource)
			)
			);

		D3DTexture2D* texture = new D3DTexture2D(
			texture_resource,
			D3D11_BIND_SHADER_RESOURCE,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN,
			false,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			);

		TCacheEntry* const entry = new TCacheEntry(
			config, texture
			);

		entry->m_texture_srv_cpu_handle = texture->GetSRV12CPU();
		entry->m_texture_srv_gpu_handle = texture->GetSRV12GPU();
		entry->m_texture_srv_gpu_handle_cpu_shadow = texture->GetSRV12GPUCPUShadow();

		// EXISTINGD3D11TODO: better debug names
		D3D::SetDebugObjectName12(entry->m_texture->GetTex12(), "a texture of the TextureCache");

		SAFE_RELEASE(texture_resource);

		return entry;
	}
}

void TextureCache::TCacheEntry::FromRenderTarget(u8* dst, PEControl::PixelFormat src_format, const EFBRectangle& srcRect,
	bool scale_by_half, unsigned int cbuf_id, const float* colmat)
{
	static unsigned int old_cbuf_id = UINT_MAX;

	// When copying at half size, in multisampled mode, resolve the color/depth buffer first.
	// This is because multisampled texture reads go through Load, not Sample, and the linear
	// filter is ignored.
	bool multisampled = (g_ActiveConfig.iMultisamples > 1);
	D3DTexture2D* efb_tex = (src_format == PEControl::Z24) ?
		FramebufferManager::GetEFBDepthTexture() :
		FramebufferManager::GetEFBColorTexture();
	if (multisampled && scale_by_half)
	{
		multisampled = false;
		efb_tex = (src_format == PEControl::Z24) ?
			FramebufferManager::GetResolvedEFBDepthTexture() :
			FramebufferManager::GetResolvedEFBColorTexture();
	}

	// stretch picture with increased internal resolution
	const D3D12_VIEWPORT vp = {
		0.f,
		0.f,
		static_cast<float>(config.width),
		static_cast<float>(config.height),
		D3D12_MIN_DEPTH,
		D3D12_MAX_DEPTH
	};

	D3D::current_command_list->RSSetViewports(1, &vp);

	// set transformation
	if (cbuf_id != old_cbuf_id)
	{
		s_efb_copy_stream_buffer->AllocateSpaceInBuffer(28 * sizeof(float), 256);

		memcpy(s_efb_copy_stream_buffer->GetCPUAddressOfCurrentAllocation(), colmat, 28 * sizeof(float));

		old_cbuf_id = cbuf_id;
	}

	D3D::current_command_list->SetGraphicsRootConstantBufferView(DESCRIPTOR_TABLE_PS_CBVONE, s_efb_copy_stream_buffer->GetGPUAddressOfCurrentAllocation());
	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PS_CBV, true);

	const TargetRectangle targetSource = g_renderer->ConvertEFBRectangle(srcRect);
	// EXISTINGD3D11TODO: try targetSource.asRECT();
	const D3D12_RECT sourcerect = CD3DX12_RECT(targetSource.left, targetSource.top, targetSource.right, targetSource.bottom);

	// Use linear filtering if (bScaleByHalf), use point filtering otherwise
	if (scale_by_half)
		D3D::SetLinearCopySampler();
	else
		D3D::SetPointCopySampler();

	// Make sure we don't draw with the texture set as both a source and target.
	// (This can happen because we don't unbind textures when we free them.)

	m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(1, &m_texture->GetRTV12(), FALSE, nullptr);

	// Create texture copy
	D3D::DrawShadedTexQuad(
		efb_tex,
		&sourcerect,
		Renderer::GetTargetWidth(),
		Renderer::GetTargetHeight(),
		(src_format == PEControl::Z24) ? StaticShaderCache::GetDepthMatrixPixelShader(multisampled) : StaticShaderCache::GetColorMatrixPixelShader(multisampled),
		StaticShaderCache::GetSimpleVertexShader(),
		StaticShaderCache::GetSimpleVertexShaderInputLayout(),
		StaticShaderCache::GetCopyGeometryShader(),
		1.0f, 0, DXGI_FORMAT_R8G8B8A8_UNORM, false, m_texture->GetMultisampled()
		);

	m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());

	g_renderer->RestoreAPIState();
}

void TextureCache::CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
	PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
	bool isIntensity, bool scaleByHalf)
{
	s_encoder->Encode(dst, format, native_width, bytes_per_row, num_blocks_y, memory_stride, srcFormat, srcRect, isIntensity, scaleByHalf);
}

static const constexpr char s_palette_shader_hlsl[] =
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
	// stretch picture with increased internal resolution
	const D3D12_VIEWPORT vp = { 0.f, 0.f, static_cast<float>(unconverted->config.width), static_cast<float>(unconverted->config.height), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	D3D::current_command_list->RSSetViewports(1, &vp);

	const unsigned int palette_buffer_allocation_size = 512;
	m_palette_stream_buffer->AllocateSpaceInBuffer(palette_buffer_allocation_size, 256);
	memcpy(m_palette_stream_buffer->GetCPUAddressOfCurrentAllocation(), palette, palette_buffer_allocation_size);

	// D3D12: Because the second SRV slot is occupied by this buffer, and an arbitrary texture occupies the first SRV slot,
	// we need to allocate temporary space out of our descriptor heap, place the palette SRV in the second slot, then copy the
	// existing texture's descriptor into the first slot.

	// First, allocate the (temporary) space in the descriptor heap.
	D3D12_CPU_DESCRIPTOR_HANDLE srv_group_cpu_handle[2] = {};
	D3D12_GPU_DESCRIPTOR_HANDLE srv_group_gpu_handle[2] = {};
	D3D::gpu_descriptor_heap_mgr->AllocateGroup(srv_group_cpu_handle, 2, srv_group_gpu_handle, nullptr, true);

	srv_group_cpu_handle[1].ptr = srv_group_cpu_handle[0].ptr + D3D::resource_descriptor_size;

	// Now, create the palette SRV at the appropriate offset.
	D3D12_SHADER_RESOURCE_VIEW_DESC palette_buffer_srv_desc = {
		DXGI_FORMAT_R16_UINT,                    // DXGI_FORMAT Format;
		D3D12_SRV_DIMENSION_BUFFER,              // D3D12_SRV_DIMENSION ViewDimension;
		D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING // UINT Shader4ComponentMapping;
	};

	// Each 'element' is two bytes since format is R16.
	palette_buffer_srv_desc.Buffer.FirstElement = m_palette_stream_buffer->GetOffsetOfCurrentAllocation() / sizeof(u16);
	palette_buffer_srv_desc.Buffer.NumElements = 256;

	D3D::device12->CreateShaderResourceView(m_palette_stream_buffer->GetBuffer(), &palette_buffer_srv_desc, srv_group_cpu_handle[1]);

	// Now, copy the existing texture's descriptor into the new temporary location.
	static_cast<TCacheEntry*>(unconverted)->m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D::device12->CopyDescriptorsSimple(
		1,
		srv_group_cpu_handle[0],
		static_cast<TCacheEntry*>(unconverted)->m_texture->GetSRV12GPUCPUShadow(),
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);

	// Finally, bind our temporary location.
	D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SRV, srv_group_gpu_handle[0]);

	// D3D11EXISTINGTODO: Add support for C14X2 format.  (Different multiplier, more palette entries.)

	// D3D12: See TextureCache::TextureCache() - because there are only two possible buffer contents here,
	// just pre-populate the data in two parts of the same upload heap.
	if ((unconverted->format & 0xf) == GX_TF_I4)
	{
		D3D::current_command_list->SetGraphicsRootConstantBufferView(DESCRIPTOR_TABLE_PS_CBVONE, m_palette_uniform_buffer->GetGPUVirtualAddress());
	}
	else
	{
		D3D::current_command_list->SetGraphicsRootConstantBufferView(DESCRIPTOR_TABLE_PS_CBVONE, m_palette_uniform_buffer->GetGPUVirtualAddress() + 256);
	}

	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PS_CBV, true);

	const D3D12_RECT source_rect = CD3DX12_RECT(0, 0, unconverted->config.width, unconverted->config.height);

	D3D::SetPointCopySampler();

	// Make sure we don't draw with the texture set as both a source and target.
	// (This can happen because we don't unbind textures when we free them.)

	static_cast<TCacheEntry*>(entry)->m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(1, &static_cast<TCacheEntry*>(entry)->m_texture->GetRTV12(), FALSE, nullptr);

	// Create texture copy
	D3D::DrawShadedTexQuad(
		static_cast<TCacheEntry*>(unconverted)->m_texture,
		&source_rect, unconverted->config.width,
		unconverted->config.height,
		m_palette_pixel_shaders[format],
		StaticShaderCache::GetSimpleVertexShader(),
		StaticShaderCache::GetSimpleVertexShaderInputLayout(),
		StaticShaderCache::GetCopyGeometryShader(),
		1.0f,
		0,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		true,
		static_cast<TCacheEntry*>(entry)->m_texture->GetMultisampled()
		);

	static_cast<TCacheEntry*>(entry)->m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE );
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());

	g_renderer->RestoreAPIState();
}

D3D12_SHADER_BYTECODE GetConvertShader12(std::string& Type)
{
	std::string shader = "#define DECODE DecodePixel_";
	shader.append(Type);
	shader.append("\n");
	shader.append(s_palette_shader_hlsl);

	ID3DBlob* blob = nullptr;
	D3D::CompilePixelShader(shader, &blob);

	return { blob->GetBufferPointer(), blob->GetBufferSize() };
}

TextureCache::TextureCache()
{
	s_encoder = std::make_unique<PSTextureEncoder>();
	s_encoder->Init();

	s_efb_copy_stream_buffer = std::make_unique<D3DStreamBuffer>(1024 * 1024, 1024 * 1024, nullptr);

	s_texture_cache_entry_readback_buffer = nullptr;
	s_texture_cache_entry_readback_buffer_data = nullptr;
	s_texture_cache_entry_readback_buffer_size = 0;

	m_palette_pixel_shaders[GX_TL_IA8] = GetConvertShader12(std::string("IA8"));
	m_palette_pixel_shaders[GX_TL_RGB565] = GetConvertShader12(std::string("RGB565"));
	m_palette_pixel_shaders[GX_TL_RGB5A3] = GetConvertShader12(std::string("RGB5A3"));

	m_palette_stream_buffer = new D3DStreamBuffer(sizeof(u16) * 256 * 1024, sizeof(u16) * 256 * 1024 * 16, nullptr);

	// Right now, there are only two variants of palette_uniform data. So, we'll just create an upload heap to permanently store both of these.
	CheckHR(
		D3D::device12->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(((16 + 255) & ~255) * 2), // Constant Buffers have to be 256b aligned. "* 2" to create for two sets of data.
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_palette_uniform_buffer)
		)
		);

	D3D::SetDebugObjectName12(m_palette_uniform_buffer, "a constant buffer used in TextureCache::ConvertTexture");

	// Temporarily repurpose m_palette_stream_buffer as a copy source to populate initial data here.
	m_palette_stream_buffer->AllocateSpaceInBuffer(256 * 2, 256);

	u8* upload_heap_data_location = reinterpret_cast<u8*>(m_palette_stream_buffer->GetCPUAddressOfCurrentAllocation());

	memset(upload_heap_data_location, 0, 256 * 2);

	float paramsFormatZero[4] = { 15.f };
	float paramsFormatNonzero[4] = { 255.f };

	memcpy(upload_heap_data_location, paramsFormatZero, sizeof(paramsFormatZero));
	memcpy(upload_heap_data_location + 256, paramsFormatNonzero, sizeof(paramsFormatNonzero));

	D3D::current_command_list->CopyBufferRegion(m_palette_uniform_buffer, 0, m_palette_stream_buffer->GetBuffer(), m_palette_stream_buffer->GetOffsetOfCurrentAllocation(), 256 * 2);

	DX12::D3D::ResourceBarrier(D3D::current_command_list, m_palette_uniform_buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, 0);
}

TextureCache::~TextureCache()
{
	s_encoder->Shutdown();
	s_encoder.reset();

	s_efb_copy_stream_buffer.reset();

	SAFE_DELETE(m_palette_stream_buffer);

	if (s_texture_cache_entry_readback_buffer)
	{
		D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(s_texture_cache_entry_readback_buffer);
		s_texture_cache_entry_readback_buffer = nullptr;
	}

	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_palette_uniform_buffer);
}

void TextureCache::BindTextures()
{
	unsigned int last_texture = 0;
	for (unsigned int i = 0; i < 8; ++i)
	{
		if (bound_textures[i] != nullptr)
		{
			last_texture = i;
		}
	}

	if (last_texture == 0 && bound_textures[0] != nullptr)
	{
		DX12::D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SRV, reinterpret_cast<TCacheEntry*>(bound_textures[0])->m_texture_srv_gpu_handle);
		return;
	}

	// If more than one texture, allocate space for group.
	D3D12_CPU_DESCRIPTOR_HANDLE s_group_base_texture_cpu_handle;
	D3D12_GPU_DESCRIPTOR_HANDLE s_group_base_texture_gpu_handle;
	DX12::D3D::gpu_descriptor_heap_mgr->AllocateGroup(&s_group_base_texture_cpu_handle, 8, &s_group_base_texture_gpu_handle, nullptr, true);

	for (unsigned int stage = 0; stage <= last_texture; stage++)
	{
		if (bound_textures[stage] != nullptr)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE textureDestDescriptor;
			textureDestDescriptor.ptr = s_group_base_texture_cpu_handle.ptr + stage * D3D::resource_descriptor_size;

			DX12::D3D::device12->CopyDescriptorsSimple(
				1,
				textureDestDescriptor,
				reinterpret_cast<TCacheEntry*>(bound_textures[stage])->m_texture_srv_gpu_handle_cpu_shadow,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
		}
		else
		{
			D3D12_CPU_DESCRIPTOR_HANDLE nullDestDescriptor;
			nullDestDescriptor.ptr = s_group_base_texture_cpu_handle.ptr + stage * D3D::resource_descriptor_size;

			DX12::D3D::device12->CopyDescriptorsSimple(
				1,
				nullDestDescriptor,
				DX12::D3D::null_srv_cpu_shadow,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
		}
	}

	// Actually bind the textures.
	DX12::D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SRV, s_group_base_texture_gpu_handle);
}

}
