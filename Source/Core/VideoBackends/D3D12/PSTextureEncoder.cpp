// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/Memmap.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DShader.h"
#include "VideoBackends/D3D12/D3DState.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/PSTextureEncoder.h"
#include "VideoBackends/D3D12/Render.h"
#include "VideoBackends/D3D12/StaticShaderCache.h"
#include "VideoBackends/D3D12/TextureCache.h"

#include "VideoCommon/TextureConversionShader.h"

namespace DX12
{

struct EFBEncodeParams
{
	DWORD SrcLeft;
	DWORD SrcTop;
	DWORD DestWidth;
	DWORD ScaleFactor;
};

PSTextureEncoder::PSTextureEncoder()
{
}

void PSTextureEncoder::Init()
{
	// Create output texture RGBA format
	D3D12_RESOURCE_DESC out_tex_desc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_B8G8R8A8_UNORM,
		EFB_WIDTH * 4,
		EFB_HEIGHT / 4,
		1,
		0,
		1,
		0,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
		);

	D3D12_CLEAR_VALUE optimized_clear_value = { DXGI_FORMAT_B8G8R8A8_UNORM, { 0.0f, 0.0f, 0.0f, 1.0f } };

	CheckHR(
		D3D::device12->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&out_tex_desc,
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			&optimized_clear_value,
			IID_PPV_ARGS(&m_out)
			)
		);

	D3D::SetDebugObjectName12(m_out, "efb encoder output texture");

	// Create output render target view
	D3D12_RENDER_TARGET_VIEW_DESC tex_rtv_desc = {
		DXGI_FORMAT_B8G8R8A8_UNORM,    // DXGI_FORMAT Format;
		D3D12_RTV_DIMENSION_TEXTURE2D  // D3D12_RTV_DIMENSION ViewDimension;
	};

	tex_rtv_desc.Texture2D.MipSlice = 0;

	D3D::rtv_descriptor_heap_mgr->Allocate(&m_out_rtv_cpu);
	D3D::device12->CreateRenderTargetView(m_out, &tex_rtv_desc, m_out_rtv_cpu);

	// Create output staging buffer
	CheckHR(
		D3D::device12->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(
				D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT +
				((out_tex_desc.Width * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)) *
				out_tex_desc.Height
				),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_out_readback_buffer)
			)
		);

	D3D::SetDebugObjectName12(m_out_readback_buffer, "efb encoder output staging buffer");

	CheckHR(m_out_readback_buffer->Map(0, nullptr, &m_out_readback_buffer_data));

	// Create constant buffer for uploading data to shaders. Need to align to 256 bytes.
	unsigned int encode_params_buffer_size = (sizeof(EFBEncodeParams) + 0xff) & ~0xff;

	CheckHR(
		D3D::device12->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(encode_params_buffer_size),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_encode_params_buffer)
			)
		);

	D3D::SetDebugObjectName12(m_encode_params_buffer, "efb encoder params buffer");

	CheckHR(m_encode_params_buffer->Map(0, nullptr, &m_encode_params_buffer_data));

	m_ready = true;
}

void PSTextureEncoder::Shutdown()
{
	m_ready = false;

	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_out);
	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_out_readback_buffer);
	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_encode_params_buffer);

	for (auto& it : m_static_shaders_blobs)
	{
		SAFE_RELEASE(it);
	}

	m_static_shaders_blobs.clear();
	m_static_shaders_map.clear();
}

void PSTextureEncoder::Encode(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
	PEControl::PixelFormat src_format, const EFBRectangle& src_rect,
	bool is_intensity, bool scale_by_half)
{
	if (!m_ready) // Make sure we initialized OK
		return;

	D3D::command_list_mgr->CPUAccessNotify();

	// Resolve MSAA targets before copying.
	D3DTexture2D* efb_source = (src_format == PEControl::Z24) ?
		FramebufferManager::GetResolvedEFBDepthTexture() :
		// EXISTINGD3D11TODO: Instead of resolving EFB, it would be better to pick out a
		// single sample from each pixel. The game may break if it isn't
		// expecting the blurred edges around multisampled shapes.
		FramebufferManager::GetResolvedEFBColorTexture();

	// GetResolvedEFBDepthTexture will set the render targets, when MSAA is enabled
	// (since it needs to do a manual depth resolve). So make sure to set the RTs
	// afterwards.

	const u32 words_per_row = bytes_per_row / sizeof(u32);

	D3D12_VIEWPORT vp = { 0.f, 0.f, FLOAT(words_per_row), FLOAT(num_blocks_y), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	D3D::current_command_list->RSSetViewports(1, &vp);

	constexpr EFBRectangle full_src_rect(0, 0, EFB_WIDTH, EFB_HEIGHT);

	TargetRectangle target_rect = g_renderer->ConvertEFBRectangle(full_src_rect);

	D3D::ResourceBarrier(D3D::current_command_list, m_out, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, 0);
	D3D::current_command_list->OMSetRenderTargets(1, &m_out_rtv_cpu, FALSE, nullptr);

	EFBEncodeParams params;
	params.SrcLeft = src_rect.left;
	params.SrcTop = src_rect.top;
	params.DestWidth = native_width;
	params.ScaleFactor = scale_by_half ? 2 : 1;

	memcpy(m_encode_params_buffer_data, &params, sizeof(params));
	D3D::current_command_list->SetGraphicsRootConstantBufferView(
		DESCRIPTOR_TABLE_PS_CBVONE,
		m_encode_params_buffer->GetGPUVirtualAddress()
		);

	D3D::command_list_mgr->m_dirty_ps_cbv = true;

	// Use linear filtering if (bScaleByHalf), use point filtering otherwise
	if (scale_by_half)
		D3D::SetLinearCopySampler();
	else
		D3D::SetPointCopySampler();

	D3D::DrawShadedTexQuad(efb_source,
		target_rect.AsRECT(),
		Renderer::GetTargetWidth(),
		Renderer::GetTargetHeight(),
		SetStaticShader(format, src_format, is_intensity, scale_by_half),
		StaticShaderCache::GetSimpleVertexShader(),
		StaticShaderCache::GetSimpleVertexShaderInputLayout(),
		D3D12_SHADER_BYTECODE(),
		1.0f,
		0,
		DXGI_FORMAT_B8G8R8A8_UNORM,
		false,
		false /* Render target is not multisampled */
		);

	// Copy to staging buffer
	D3D12_BOX src_box = CD3DX12_BOX(0, 0, 0, words_per_row, num_blocks_y, 1);

	D3D12_TEXTURE_COPY_LOCATION dst_location = {};
	dst_location.pResource = m_out_readback_buffer;
	dst_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst_location.PlacedFootprint.Offset = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
	dst_location.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	dst_location.PlacedFootprint.Footprint.Width = EFB_WIDTH * 4;
	dst_location.PlacedFootprint.Footprint.Height = EFB_HEIGHT / 4;
	dst_location.PlacedFootprint.Footprint.Depth = 1;
	dst_location.PlacedFootprint.Footprint.RowPitch = dst_location.PlacedFootprint.Footprint.Width * 4 /* width * 32bpp */;
	dst_location.PlacedFootprint.Footprint.RowPitch = (dst_location.PlacedFootprint.Footprint.RowPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);

	D3D12_TEXTURE_COPY_LOCATION src_location = {};
	src_location.pResource = m_out;
	src_location.SubresourceIndex = 0;
	src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

	D3D::ResourceBarrier(D3D::current_command_list, m_out, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
	D3D::current_command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, &src_box);

	D3D::command_list_mgr->ExecuteQueuedWork(true);

	// Transfer staging buffer to GameCube/Wii RAM

	u8* src = static_cast<u8*>(m_out_readback_buffer_data) + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
	u32 read_stride = std::min(bytes_per_row, dst_location.PlacedFootprint.Footprint.RowPitch);
	for (unsigned int y = 0; y < num_blocks_y; ++y)
	{
		memcpy(dst, src, read_stride);

		dst += memory_stride;
		src += dst_location.PlacedFootprint.Footprint.RowPitch;
	}

	// Restores proper viewport/scissor settings.
	g_renderer->RestoreAPIState();

	FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE );
	D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());
}

D3D12_SHADER_BYTECODE PSTextureEncoder::SetStaticShader(unsigned int dst_format, PEControl::PixelFormat src_format,
	bool is_intensity, bool scale_by_half)
{
	size_t fetch_num = static_cast<size_t>(src_format);
	size_t scaled_fetch_num = scale_by_half ? 1 : 0;
	size_t intensity_num = is_intensity ? 1 : 0;
	size_t generator_num = dst_format;

	ComboKey key = MakeComboKey(dst_format, src_format, is_intensity, scale_by_half);

	ComboMap::iterator it = m_static_shaders_map.find(key);
	if (it == m_static_shaders_map.end())
	{
		INFO_LOG(VIDEO, "Compiling efb encoding shader for dst_format 0x%X, src_format %d, is_intensity %d, scale_by_half %d",
			dst_format, static_cast<int>(src_format), is_intensity ? 1 : 0, scale_by_half ? 1 : 0);

		u32 format = dst_format;

		if (src_format == PEControl::Z24)
		{
			format |= _GX_TF_ZTF;
			if (dst_format == 11)
				format = GX_TF_Z16;
			else if (format < GX_TF_Z8 || format > GX_TF_Z24X8)
				format |= _GX_TF_CTF;
		}
		else
		{
			if (dst_format > GX_TF_RGBA8 || (dst_format < GX_TF_RGB565 && !is_intensity))
				format |= _GX_TF_CTF;
		}

		D3DBlob* bytecode = nullptr;
		const char* shader = TextureConversionShader::GenerateEncodingShader(format, API_D3D);
		if (!D3D::CompilePixelShader(shader, &bytecode))
		{
			WARN_LOG(VIDEO, "EFB encoder shader for dst_format 0x%X, src_format %d, is_intensity %d, scale_by_half %d failed to compile",
				dst_format, static_cast<int>(src_format), is_intensity ? 1 : 0, scale_by_half ? 1 : 0);
			m_static_shaders_blobs[key] = {};
			return {};
		}

		D3D12_SHADER_BYTECODE new_shader = {
			bytecode->Data(),
			bytecode->Size()
		};

		it = m_static_shaders_map.emplace(key, new_shader).first;

		// Keep track of the D3DBlobs, so we can free them upon shutdown.
		m_static_shaders_blobs.push_back(bytecode);
	}

	return it->second;
}

}
