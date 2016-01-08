// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DTexture.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/Render.h"

namespace DX12
{

namespace D3D
{

static ID3D12Resource* s_texture_upload_heaps[2] = {};
void* s_texture_upload_heap_data_pointers[2] = {};

unsigned int s_texture_upload_heap_current_index = 0;
unsigned int s_texture_upload_heap_current_offset = 0;

static unsigned int s_texture_upload_heap_size = 8 * 1024 * 1024;

void MoveToNextD3DTextureUploadHeap()
{
	s_texture_upload_heap_current_index =
		(s_texture_upload_heap_current_index + 1) % ARRAYSIZE(s_texture_upload_heaps);

	s_texture_upload_heap_current_offset = 0;
}

void CleanupPersistentD3DTextureResources()
{
	// All GPU work has stopped at this point, can destroy immediately.

	for (UINT i = 0; i < ARRAYSIZE(s_texture_upload_heaps); i++)
	{
		SAFE_RELEASE(s_texture_upload_heaps[i]);
		s_texture_upload_heap_data_pointers[i] = nullptr;
	}

	s_texture_upload_heap_current_index = 0;
	s_texture_upload_heap_current_offset = 0;
}

void ReplaceRGBATexture2D(ID3D12Resource* texture12, const u8* buffer, unsigned int width, unsigned int height, unsigned int src_pitch, unsigned int level, D3D12_RESOURCE_STATES current_resource_state)
{
	unsigned int upload_size =
		D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT +
		((src_pitch + (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)) * height;

	if (s_texture_upload_heap_current_offset + upload_size > s_texture_upload_heap_size)
	{
		// Ran out of room.. so wait for GPU queue to drain, free resource, then reallocate buffers with double the size.

		for (unsigned int i = 0; i < ARRAYSIZE(s_texture_upload_heaps); i++)
		{
			if (s_texture_upload_heaps[i])
			{
				D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(s_texture_upload_heaps[i]);
				s_texture_upload_heaps[i] = nullptr;
			}
		}

		D3D::command_list_mgr->ExecuteQueuedWork(true);

		// Reset state to defaults.
		g_renderer->SetViewport();
		D3D::current_command_list->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV12(), FALSE, &FramebufferManager::GetEFBDepthTexture()->GetDSV12());

		s_texture_upload_heap_size = std::max(s_texture_upload_heap_size * 2, upload_size);
		s_texture_upload_heap_current_offset = 0;
	}

	if (!s_texture_upload_heaps[s_texture_upload_heap_current_index])
	{
		CheckHR(
			device12->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(s_texture_upload_heap_size),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&s_texture_upload_heaps[s_texture_upload_heap_current_index])
				)
			);
	}

	D3D12_SUBRESOURCE_DATA subresource_data = {
		buffer,    // const void *pData;
		src_pitch, // LONG_PTR RowPitch;
		0,         // LONG_PTR SlicePitch;
	};

	ResourceBarrier(current_command_list, texture12, current_resource_state, D3D12_RESOURCE_STATE_COPY_DEST, level);

	CHECK(0 != UpdateSubresources(current_command_list, texture12, s_texture_upload_heaps[s_texture_upload_heap_current_index], s_texture_upload_heap_current_offset, level, 1, &subresource_data), "UpdateSubresources failed.");

	ResourceBarrier(D3D::current_command_list, texture12, D3D12_RESOURCE_STATE_COPY_DEST, current_resource_state, level);

	s_texture_upload_heap_current_offset += upload_size;

	s_texture_upload_heap_current_offset = (s_texture_upload_heap_current_offset + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1); // Align offset in upload heap to 512 bytes, as required.
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
	++m_ref;
}

UINT D3DTexture2D::Release()
{
	--m_ref;
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
