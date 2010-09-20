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

#include "DX11_D3DBase.h"

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

	D3DTexture2D(ID3D11Texture2D* texptr, D3D11_BIND_FLAG bind, DXGI_FORMAT srv_format = DXGI_FORMAT_UNKNOWN, DXGI_FORMAT dsv_format = DXGI_FORMAT_UNKNOWN, DXGI_FORMAT rtv_format = DXGI_FORMAT_UNKNOWN);
	static D3DTexture2D* Create(unsigned int width, unsigned int height, D3D11_BIND_FLAG bind, D3D11_USAGE usage, DXGI_FORMAT, unsigned int levels = 1);

	// reference counting, use AddRef() when creating a new reference and Release() it when you don't need it anymore
	void AddRef();
	UINT Release();

	ID3D11Texture2D* &GetTex() { return tex; }
	ID3D11ShaderResourceView* &GetSRV() { return srv; }
	ID3D11RenderTargetView* &GetRTV() { return rtv; }
	ID3D11DepthStencilView* &GetDSV() { return dsv; }

private:
	~D3DTexture2D();

	ID3D11Texture2D* tex;
	ID3D11ShaderResourceView* srv;
	ID3D11RenderTargetView* rtv;
	ID3D11DepthStencilView* dsv;
	D3D11_BIND_FLAG bindflags;
	UINT ref;
};

}
