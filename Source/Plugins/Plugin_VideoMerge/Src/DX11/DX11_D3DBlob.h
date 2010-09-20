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

#include "Common.h"
#include <d3d11.h>

namespace DX11
{

// use this class instead ID3D10Blob or ID3D11Blob whenever possible
class D3DBlob
{
public:
	// memory will be copied into an own buffer
	D3DBlob(unsigned int blob_size, const u8* init_data = NULL);

	// d3dblob will be AddRef'd
	D3DBlob(ID3D10Blob* d3dblob);

	void AddRef();
	unsigned int Release();

	unsigned int Size();
	u8* Data();

private:
	~D3DBlob();

	unsigned int ref;
	unsigned int size;

	u8* data;
	ID3D10Blob* blob;
};

}
