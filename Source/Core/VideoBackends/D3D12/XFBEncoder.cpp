// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/Logging/Log.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DShader.h"
#include "VideoBackends/D3D12/D3DState.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/Render.h"
#include "VideoBackends/D3D12/StaticShaderCache.h"
#include "VideoBackends/D3D12/XFBEncoder.h"

namespace DX12
{

// YUYV data is packed into half-width RGBA, with Y values in (R,B) and UV in (G,A)
constexpr size_t XFB_TEXTURE_WIDTH = MAX_XFB_WIDTH / 2;
constexpr size_t XFB_TEXTURE_HEIGHT = MAX_XFB_HEIGHT;

// Buffer enough space for 2 XFB buffers (our frame latency)
constexpr size_t XFB_UPLOAD_BUFFER_SIZE = D3D::AlignValue(XFB_TEXTURE_WIDTH * sizeof(u32), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * XFB_TEXTURE_HEIGHT * 2;
constexpr size_t XFB_ENCODER_PARAMS_BUFFER_SIZE = 64 * 1024;

std::unique_ptr<XFBEncoder> g_xfb_encoder;

XFBEncoder::XFBEncoder()
{
	ID3D12Resource* texture;

	CheckHR(D3D::device12->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, XFB_TEXTURE_WIDTH, XFB_TEXTURE_HEIGHT, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		nullptr,
		IID_PPV_ARGS(&texture)));

	m_yuyv_texture = new D3DTexture2D(texture,
		TEXTURE_BIND_FLAG_SHADER_RESOURCE | TEXTURE_BIND_FLAG_RENDER_TARGET,
		DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UNORM);
	SAFE_RELEASE(texture);

	CheckHR(D3D::device12->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(D3D::AlignValue(XFB_TEXTURE_WIDTH * sizeof(u32), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * MAX_XFB_HEIGHT),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_readback_buffer)));

	m_upload_buffer = std::make_unique<D3DStreamBuffer>(XFB_UPLOAD_BUFFER_SIZE, XFB_UPLOAD_BUFFER_SIZE, nullptr);
	m_encode_params_buffer = std::make_unique<D3DStreamBuffer>(XFB_ENCODER_PARAMS_BUFFER_SIZE, XFB_ENCODER_PARAMS_BUFFER_SIZE, nullptr);
}

XFBEncoder::~XFBEncoder()
{
	SAFE_RELEASE(m_yuyv_texture);
	SAFE_RELEASE(m_readback_buffer);
}

void XFBEncoder::EncodeTextureToRam(u8* dst, u32 dst_pitch, u32 dst_height,
	                                D3DTexture2D* src_texture, const TargetRectangle& src_rect,
	                                u32 src_width, u32 src_height, float gamma)
{
	// src_rect is in native coordinates
	// dst_pitch is in words
	u32 dst_width = dst_pitch / 2;
	u32 dst_texture_width = dst_width / 2;
	_assert_msg_(VIDEO, dst_width <= MAX_XFB_WIDTH && dst_height <= MAX_XFB_HEIGHT, "XFB destination does not exceed maximum size");

	// Encode parameters constant buffer used by shader
	struct EncodeParameters
	{
		float srcRect[4];
		float texelSize[2];
		float pad[2];
	};
	EncodeParameters parameters =
	{
		{
			static_cast<float>(src_rect.left) / static_cast<float>(src_width),
			static_cast<float>(src_rect.top) / static_cast<float>(src_height),
			static_cast<float>(src_rect.right) / static_cast<float>(src_width),
			static_cast<float>(src_rect.bottom) / static_cast<float>(src_height)
		},
		{
			1.0f / EFB_WIDTH,
			1.0f / EFB_HEIGHT
		},
		{
			0.0f,
			0.0f
		}
	};
	m_encode_params_buffer->AllocateSpaceInBuffer(sizeof(parameters), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	memcpy(m_encode_params_buffer->GetCPUAddressOfCurrentAllocation(), &parameters, sizeof(parameters));

	// Convert RGBA texture to YUYV intermediate texture.
	// Performs downscaling through a linear filter. Probably not ideal, but it's not going to look perfect anyway.
	CD3DX12_RECT src_texture_rect(src_rect.left, src_rect.top, src_rect.right, src_rect.bottom);
	D3D12_RESOURCE_STATES src_texture_state = src_texture->GetResourceUsageState();
	m_yuyv_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(1, &m_yuyv_texture->GetRTV12(), FALSE, nullptr);
	D3D::current_command_list->SetGraphicsRootConstantBufferView(DESCRIPTOR_TABLE_PS_CBVONE, m_encode_params_buffer->GetGPUAddressOfCurrentAllocation());
	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PS_CBV, true);
	D3D::SetViewportAndScissor(0, 0, dst_texture_width, dst_height);
	D3D::SetLinearCopySampler();
	D3D::DrawShadedTexQuad(
		src_texture, &src_texture_rect, src_rect.GetWidth(), src_rect.GetHeight(),
		StaticShaderCache::GetXFBEncodePixelShader(), StaticShaderCache::GetSimpleVertexShader(), StaticShaderCache::GetSimpleVertexShaderInputLayout(),
		{}, gamma, 0, DXGI_FORMAT_R8G8B8A8_UNORM, false, false);

	src_texture->TransitionToResourceState(D3D::current_command_list, src_texture_state);

	// Copy from YUYV intermediate texture to readback buffer. It's likely the pitch here is going to be different to dst_pitch.
	u32 readback_pitch = D3D::AlignValue(dst_width * sizeof(u16), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT dst_footprint = { 0, { DXGI_FORMAT_R8G8B8A8_UNORM, dst_texture_width, dst_height, 1, readback_pitch } };
	CD3DX12_TEXTURE_COPY_LOCATION dst_location(m_readback_buffer, dst_footprint);
	CD3DX12_TEXTURE_COPY_LOCATION src_location(m_yuyv_texture->GetTex12(), 0);
	CD3DX12_BOX src_box(0, 0, dst_texture_width, dst_height);
	m_yuyv_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
	D3D::current_command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, &src_box);

	// Wait until the GPU completes the copy. Resets back to known state automatically.
	D3D::command_list_mgr->ExecuteQueuedWork(true);

	// Copy from the readback buffer to dst.
	// Can't be done as one memcpy due to pitch difference.
	void* readback_texture_map;
	D3D12_RANGE read_range = { 0, readback_pitch * dst_height };
	CheckHR(m_readback_buffer->Map(0, &read_range, &readback_texture_map));

	for (u32 row = 0; row < dst_height; row++)
	{
		const u8* row_src = reinterpret_cast<u8*>(readback_texture_map) + readback_pitch * row;
		u8* row_dst = dst + dst_pitch * row;
		memcpy(row_dst, row_src, std::min(dst_pitch, readback_pitch));
	}

	D3D12_RANGE write_range = {};
	m_readback_buffer->Unmap(0, &write_range);
}

void XFBEncoder::DecodeToTexture(D3DTexture2D* dst_texture, const u8* src, u32 src_width, u32 src_height)
{
	_assert_msg_(VIDEO, src_width <= MAX_XFB_WIDTH && src_height <= MAX_XFB_HEIGHT, "XFB source does not exceed maximum size");

	// Copy to XFB upload buffer. Each row has to be done separately due to pitch differences.
	u32 buffer_pitch = D3D::AlignValue(src_width / 2 * sizeof(u32), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	m_upload_buffer->AllocateSpaceInBuffer(buffer_pitch * src_height, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
	for (u32 row = 0; row < src_height; row++)
	{
		const u8* row_src = src + (src_width * 2) * row;
		u8* row_dst = reinterpret_cast<u8*>(m_upload_buffer->GetCPUAddressOfCurrentAllocation()) + buffer_pitch * row;
		memcpy(row_dst, row_src, src_width * 2);
	}

	// Copy from upload buffer to intermediate YUYV texture.
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT src_footprint = { m_upload_buffer->GetOffsetOfCurrentAllocation(), { DXGI_FORMAT_R8G8B8A8_UNORM, src_width / 2, src_height, 1, buffer_pitch } };
	CD3DX12_TEXTURE_COPY_LOCATION src_location(m_upload_buffer->GetBuffer(), src_footprint);
	CD3DX12_TEXTURE_COPY_LOCATION dst_location(m_yuyv_texture->GetTex12(), 0);
	CD3DX12_BOX src_box(0, 0, src_width / 2, src_height);
	m_yuyv_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_DEST);
	D3D::current_command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, &src_box);

	// Convert YUYV texture to RGBA texture with pixel shader.
	CD3DX12_RECT src_texture_rect(0, 0, src_width / 2, src_height);
	dst_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D::current_command_list->OMSetRenderTargets(1, &dst_texture->GetRTV12(), FALSE, nullptr);
	D3D::SetViewportAndScissor(0, 0, src_width, src_height);
	D3D::DrawShadedTexQuad(
		m_yuyv_texture, &src_texture_rect, XFB_TEXTURE_WIDTH, XFB_TEXTURE_HEIGHT,
		StaticShaderCache::GetXFBDecodePixelShader(), StaticShaderCache::GetSimpleVertexShader(), StaticShaderCache::GetSimpleVertexShaderInputLayout(),
		{}, 1.0f, 0, DXGI_FORMAT_R8G8B8A8_UNORM, false, false);

	// XFB source textures are expected to be in shader resource state.
	dst_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

}
