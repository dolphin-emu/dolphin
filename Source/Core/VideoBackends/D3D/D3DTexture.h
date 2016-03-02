// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>

namespace DX11
{

namespace D3D
{
	void ReplaceRGBATexture2D(ID3D11Texture2D* pTexture, const u8* buffer, unsigned int width, unsigned int height, unsigned int src_pitch, unsigned int level, D3D11_USAGE usage);
}

class D3DTexture2D
{
public:
	void Create(DXGI_FORMAT format, unsigned int width, unsigned int height, 
		D3D11_BIND_FLAG bind, D3D11_USAGE usage = D3D11_USAGE_DEFAULT,
		unsigned int levels = 1, unsigned int slices = 1,
		const D3D11_SUBRESOURCE_DATA* data = nullptr);

	// Create from an existing ID3D11Texture2D. This function calls AddRef on
	// the texture. The caller should Release their pointer.
	void CreateFromExisting(ID3D11Texture2D* tex, D3D11_BIND_FLAG bind,
		DXGI_FORMAT srv_format = DXGI_FORMAT_UNKNOWN,
		DXGI_FORMAT dsv_format = DXGI_FORMAT_UNKNOWN,
		DXGI_FORMAT rtv_format = DXGI_FORMAT_UNKNOWN, bool multisampled = false);

	void Reset();

	ID3D11Texture2D* GetTex();
	ID3D11ShaderResourceView* GetSRV();
	ID3D11RenderTargetView* GetRTV();
	ID3D11DepthStencilView* GetDSV();

private:
	ComPtr<ID3D11Texture2D> m_tex;
	ComPtr<ID3D11ShaderResourceView> m_srv;
	ComPtr<ID3D11RenderTargetView> m_rtv;
	ComPtr<ID3D11DepthStencilView> m_dsv;
};

ComPtr<ID3D11Texture2D> CreateStagingTexture(DXGI_FORMAT format,
	unsigned int width, unsigned int height,
	unsigned int levels = 1, unsigned int slices = 1,
	D3D11_CPU_ACCESS_FLAG cpuAccess = D3D11_CPU_ACCESS_READ);

}  // namespace DX11
