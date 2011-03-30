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

#pragma once

#include <d3d11.h>

namespace DX11
{

namespace D3D
{
	void ReplaceRGBATexture2D(ID3D11Texture2D* pTexture, const u8* buffer, unsigned int width, unsigned int height, unsigned int pitch, unsigned int level, D3D11_USAGE usage);
}

class D3DTexture2D
{
public:
	// there are two ways to create a D3DTexture2D object:
	//     either create an ID3D11Texture2D object, pass it to the constructor and specify what views to create
	//     or let the texture automatically be created by D3DTexture2D::Create

	D3DTexture2D(SharedPtr<ID3D11Texture2D> texptr, D3D11_BIND_FLAG bind,
		DXGI_FORMAT srv_format = DXGI_FORMAT_UNKNOWN, DXGI_FORMAT dsv_format = DXGI_FORMAT_UNKNOWN,
		DXGI_FORMAT rtv_format = DXGI_FORMAT_UNKNOWN, bool multisampled = false);

	static std::unique_ptr<D3DTexture2D> Create(unsigned int width, unsigned int height,
		D3D11_BIND_FLAG bind, D3D11_USAGE usage, DXGI_FORMAT, unsigned int levels = 1);

	ID3D11Texture2D* GetTex() { return tex; }
	ID3D11ShaderResourceView*const& GetSRV() { return srv; }
	ID3D11RenderTargetView*const& GetRTV() { return rtv; }
	ID3D11DepthStencilView*const& GetDSV() { return dsv; }

	~D3DTexture2D();

private:
	SharedPtr<ID3D11Texture2D> tex;
	ID3D11ShaderResourceView* srv;
	ID3D11RenderTargetView* rtv;
	ID3D11DepthStencilView* dsv;
	D3D11_BIND_FLAG bindflags;
	UINT ref;
};

}  // namespace DX11