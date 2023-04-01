// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>

namespace DX12
{

enum TEXTURE_BIND_FLAG : u32
{
	TEXTURE_BIND_FLAG_SHADER_RESOURCE           = (1 << 0),
	TEXTURE_BIND_FLAG_RENDER_TARGET             = (1 << 1),
	TEXTURE_BIND_FLAG_DEPTH_STENCIL             = (1 << 2)
};

namespace D3D
{
	void ReplaceRGBATexture2D(ID3D12Resource* pTexture, const u8* buffer, unsigned int width, unsigned int height, unsigned int src_pitch, unsigned int level, D3D12_RESOURCE_STATES current_resource_state = D3D12_RESOURCE_STATE_COMMON);
	void CleanupPersistentD3DTextureResources();
}

class D3DTexture2D
{

public:
	// there are two ways to create a D3DTexture2D object:
	//     either create an ID3D12Resource object, pass it to the constructor and specify what views to create
	//     or let the texture automatically be created by D3DTexture2D::Create

	D3DTexture2D(ID3D12Resource* texptr, u32 bind, DXGI_FORMAT srv_format = DXGI_FORMAT_UNKNOWN, DXGI_FORMAT dsv_format = DXGI_FORMAT_UNKNOWN, DXGI_FORMAT rtv_format = DXGI_FORMAT_UNKNOWN, bool multisampled = false, D3D12_RESOURCE_STATES resource_state = D3D12_RESOURCE_STATE_COMMON);
	static D3DTexture2D* Create(unsigned int width, unsigned int height, u32 bind, DXGI_FORMAT fmt, unsigned int levels = 1, unsigned int slices = 1, D3D12_SUBRESOURCE_DATA* data = nullptr);
	void TransitionToResourceState(ID3D12GraphicsCommandList* command_list, D3D12_RESOURCE_STATES state_after);

	// reference counting, use AddRef() when creating a new reference and Release() it when you don't need it anymore
	void AddRef();
	UINT Release();

	ID3D12Resource* GetTex12() const;

	D3D12_CPU_DESCRIPTOR_HANDLE GetSRV12CPU() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRV12GPU() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRV12GPUCPUShadow() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSV12() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTV12() const;

	D3D12_RESOURCE_STATES GetResourceUsageState() const;

	bool GetMultisampled() const;

private:
	~D3DTexture2D();

	ID3D12Resource* m_tex12 = nullptr;

	D3D12_CPU_DESCRIPTOR_HANDLE m_srv12_cpu = {};
	D3D12_GPU_DESCRIPTOR_HANDLE m_srv12_gpu = {};
	D3D12_CPU_DESCRIPTOR_HANDLE m_srv12_gpu_cpu_shadow = {};

	D3D12_CPU_DESCRIPTOR_HANDLE m_dsv12 = {};
	D3D12_CPU_DESCRIPTOR_HANDLE m_rtv12 = {};

	D3D12_RESOURCE_STATES m_resource_state = D3D12_RESOURCE_STATE_COMMON;

	bool m_multisampled = false;

	std::atomic<unsigned long> m_ref = 1;
};

}  // namespace DX12
