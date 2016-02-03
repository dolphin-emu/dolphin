// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DStreamBuffer.h"
#include "VideoBackends/D3D12/D3DTexture.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/Render.h"

namespace DX12
{

namespace D3D
{

static D3DStreamBuffer* s_texture_upload_stream_buffer = nullptr;

void CleanupPersistentD3DTextureResources()
{
	SAFE_DELETE(s_texture_upload_stream_buffer);
}

void ReplaceRGBATexture2D(ID3D12Resource* texture12, const u8* buffer, unsigned int width, unsigned int height, unsigned int src_pitch, unsigned int level, D3D12_RESOURCE_STATES current_resource_state)
{
	const unsigned int upload_size = AlignValue(src_pitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * height;

	if (!s_texture_upload_stream_buffer)
	{
		s_texture_upload_stream_buffer = new D3DStreamBuffer(4 * 1024 * 1024, 64 * 1024 * 1024, nullptr);
	}

	bool current_command_list_executed = s_texture_upload_stream_buffer->AllocateSpaceInBuffer(upload_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
	if (current_command_list_executed)
	{
		g_renderer->SetViewport();
		D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());
	}

	ResourceBarrier(current_command_list, texture12, current_resource_state, D3D12_RESOURCE_STATE_COPY_DEST, level);

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT upload_footprint = {};
	u32 upload_rows = 0;
	u64 upload_row_size_in_bytes = 0;
	u64 upload_total_bytes = 0;

	D3D::device12->GetCopyableFootprints(&texture12->GetDesc(), level, 1, s_texture_upload_stream_buffer->GetOffsetOfCurrentAllocation(), &upload_footprint, &upload_rows, &upload_row_size_in_bytes, &upload_total_bytes);

	u8* dest_data = reinterpret_cast<u8*>(s_texture_upload_stream_buffer->GetCPUAddressOfCurrentAllocation());
	const u8* src_data = reinterpret_cast<const u8*>(buffer);
	for (u32 y = 0; y < upload_rows; ++y)
	{
		memcpy(
			dest_data + upload_footprint.Footprint.RowPitch * y,
			src_data + src_pitch * y,
			upload_row_size_in_bytes
			);
	}

	D3D::current_command_list->CopyTextureRegion(&CD3DX12_TEXTURE_COPY_LOCATION(texture12, level), 0, 0, 0, &CD3DX12_TEXTURE_COPY_LOCATION(s_texture_upload_stream_buffer->GetBuffer(), upload_footprint), nullptr);

	ResourceBarrier(D3D::current_command_list, texture12, D3D12_RESOURCE_STATE_COPY_DEST, current_resource_state, level);
}

}  // namespace

D3DTexture2D* D3DTexture2D::Create(unsigned int width, unsigned int height, D3D11_BIND_FLAG bind, D3D11_USAGE usage, DXGI_FORMAT fmt, unsigned int levels, unsigned int slices, D3D12_SUBRESOURCE_DATA* data)
{
	ID3D12Resource* texture12 = nullptr;

	D3D12_RESOURCE_DESC texdesc12 = CD3DX12_RESOURCE_DESC::Tex2D(
		fmt,
		width,
		height,
		slices,
		levels
		);

	D3D12_CLEAR_VALUE optimized_clear_value = {};
	optimized_clear_value.Format = fmt;

	if (bind & D3D11_BIND_RENDER_TARGET)
	{
		texdesc12.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		optimized_clear_value.Color[0] = 0.0f;
		optimized_clear_value.Color[1] = 0.0f;
		optimized_clear_value.Color[2] = 0.0f;
		optimized_clear_value.Color[3] = 1.0f;
	}

	if (bind & D3D11_BIND_DEPTH_STENCIL)
	{
		texdesc12.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		optimized_clear_value.DepthStencil.Depth = 0.0f;
		optimized_clear_value.DepthStencil.Stencil = 0;
	}

	CheckHR(
		D3D::device12->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC(texdesc12),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&optimized_clear_value,
			IID_PPV_ARGS(&texture12)
			)
		);

	D3D::SetDebugObjectName12(texture12, "Texture created via D3DTexture2D::Create");
	D3DTexture2D* ret = new D3DTexture2D(texture12, bind, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, false, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	if (data)
	{
		DX12::D3D::ReplaceRGBATexture2D(texture12, reinterpret_cast<const u8*>(data->pData), width, height, static_cast<unsigned int>(data->RowPitch), 0, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	SAFE_RELEASE(texture12);
	return ret;
}

void D3DTexture2D::AddRef()
{
	InterlockedIncrement(&m_ref);
}

UINT D3DTexture2D::Release()
{
	InterlockedDecrement(&m_ref);
	if (m_ref == 0)
	{
		delete this;
		return 0;
	}
	return m_ref;
}

D3D12_RESOURCE_STATES D3DTexture2D::GetResourceUsageState() const
{
	return m_resource_state;
}

bool D3DTexture2D::GetMultisampled() const
{
	return m_multisampled;
}

ID3D12Resource* D3DTexture2D::GetTex12() const
{
	return m_tex12;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DTexture2D::GetSRV12CPU() const
{
	return m_srv12_cpu;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3DTexture2D::GetSRV12GPU() const
{
	return m_srv12_gpu;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DTexture2D::GetSRV12GPUCPUShadow() const
{
	return m_srv12_gpu_cpu_shadow;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DTexture2D::GetDSV12() const
{
	return m_dsv12;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DTexture2D::GetRTV12() const
{
	return m_rtv12;
}

D3DTexture2D::D3DTexture2D(ID3D12Resource* texptr, D3D11_BIND_FLAG bind,
							DXGI_FORMAT srv_format, DXGI_FORMAT dsv_format, DXGI_FORMAT rtv_format, bool multisampled, D3D12_RESOURCE_STATES resource_state)
							: m_tex12(texptr), m_resource_state(resource_state), m_multisampled(multisampled)
{
	D3D12_SRV_DIMENSION srv_dim12 = multisampled ? D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	D3D12_DSV_DIMENSION dsv_dim12 = multisampled ? D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY : D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
	D3D12_RTV_DIMENSION rtv_dim12 = multisampled ? D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D12_RTV_DIMENSION_TEXTURE2DARRAY;

	if (bind & D3D11_BIND_SHADER_RESOURCE)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
			srv_format, // DXGI_FORMAT Format
			srv_dim12   // D3D12_SRV_DIMENSION ViewDimension
		};

		if (srv_dim12 == D3D12_SRV_DIMENSION_TEXTURE2DARRAY)
		{
			srv_desc.Texture2DArray.MipLevels = -1;
			srv_desc.Texture2DArray.MostDetailedMip = 0;
			srv_desc.Texture2DArray.ResourceMinLODClamp = 0;
			srv_desc.Texture2DArray.ArraySize = -1;
		}
		else
		{
			srv_desc.Texture2DMSArray.ArraySize = -1;
		}

		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		CHECK(D3D::gpu_descriptor_heap_mgr->Allocate(&m_srv12_cpu, &m_srv12_gpu, &m_srv12_gpu_cpu_shadow), "Error: Ran out of permenant slots in GPU descriptor heap, but don't support rolling over heap.");

		D3D::device12->CreateShaderResourceView(m_tex12, &srv_desc, m_srv12_cpu);
		D3D::device12->CreateShaderResourceView(m_tex12, &srv_desc, m_srv12_gpu_cpu_shadow);
	}

	if (bind & D3D11_BIND_DEPTH_STENCIL)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {
			dsv_format,          // DXGI_FORMAT Format
			dsv_dim12,           // D3D12_DSV_DIMENSION
			D3D12_DSV_FLAG_NONE  // D3D12_DSV_FLAG Flags
		};

		if (dsv_dim12 == D3D12_DSV_DIMENSION_TEXTURE2DARRAY)
			dsv_desc.Texture2DArray.ArraySize = -1;
		else
			dsv_desc.Texture2DMSArray.ArraySize = -1;

		D3D::dsv_descriptor_heap_mgr->Allocate(&m_dsv12);
		D3D::device12->CreateDepthStencilView(m_tex12, &dsv_desc, m_dsv12);
	}

	if (bind & D3D11_BIND_RENDER_TARGET)
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {
			rtv_format, // DXGI_FORMAT Format
			rtv_dim12   // D3D12_RTV_DIMENSION ViewDimension
		};

		if (rtv_dim12 == D3D12_RTV_DIMENSION_TEXTURE2DARRAY)
			rtv_desc.Texture2DArray.ArraySize = -1;
		else
			rtv_desc.Texture2DMSArray.ArraySize = -1;

		D3D::rtv_descriptor_heap_mgr->Allocate(&m_rtv12);
		D3D::device12->CreateRenderTargetView(m_tex12, &rtv_desc, m_rtv12);
	}

	m_tex12->AddRef();
}

void D3DTexture2D::TransitionToResourceState(ID3D12GraphicsCommandList* command_list, D3D12_RESOURCE_STATES state_after)
{
	DX12::D3D::ResourceBarrier(command_list, m_tex12, m_resource_state, state_after, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	m_resource_state = state_after;
}

D3DTexture2D::~D3DTexture2D()
{
	DX12::D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_tex12);

	if (m_srv12_cpu.ptr)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC null_srv_desc = {};
		null_srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

		null_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		DX12::D3D::device12->CreateShaderResourceView(NULL, &null_srv_desc, m_srv12_cpu);
	}
}

}  // namespace DX12
