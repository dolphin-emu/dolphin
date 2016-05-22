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
	// there are two ways to create a D3DTexture2D object:
	//     either create an ID3D11Texture2D object, pass it to the constructor and specify what views to create
	//     or let the texture automatically be created by D3DTexture2D::Create

	D3DTexture2D(ID3D11Texture2D* texptr, D3D11_BIND_FLAG bind, DXGI_FORMAT srv_format = DXGI_FORMAT_UNKNOWN, DXGI_FORMAT dsv_format = DXGI_FORMAT_UNKNOWN, DXGI_FORMAT rtv_format = DXGI_FORMAT_UNKNOWN, bool multisampled = false);
	static D3DTexture2D* Create(unsigned int width, unsigned int height, D3D11_BIND_FLAG bind, D3D11_USAGE usage, DXGI_FORMAT, unsigned int levels = 1, unsigned int slices = 1, D3D11_SUBRESOURCE_DATA* data = nullptr);

	// reference counting, use AddRef() when creating a new reference and Release() it when you don't need it anymore
	void AddRef();
	UINT Release();

	ID3D11Texture2D* &GetTex();
	ID3D11ShaderResourceView* &GetSRV();
	ID3D11RenderTargetView* &GetRTV();
	ID3D11DepthStencilView* &GetDSV();

private:
	~D3DTexture2D();

	ID3D11Texture2D* tex;
	ID3D11ShaderResourceView* srv;
	ID3D11RenderTargetView* rtv;
	ID3D11DepthStencilView* dsv;
	D3D11_BIND_FLAG bindflags;
	UINT ref;
};

}  // namespace DX11
