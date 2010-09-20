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

#include "DX11_D3DBlob.h"

namespace DX11
{

D3DBlob::D3DBlob(unsigned int blob_size, const u8* init_data) : ref(1), size(blob_size), blob(NULL)
{
	data = new u8[blob_size];
	if (init_data) memcpy(data, init_data, size);
}

D3DBlob::D3DBlob(ID3D10Blob* d3dblob) : ref(1)
{
	blob = d3dblob;
	data = (u8*)blob->GetBufferPointer();
	size = blob->GetBufferSize();
	d3dblob->AddRef();
}

D3DBlob::~D3DBlob()
{
	if (blob) blob->Release();
	else delete[] data;
}

void D3DBlob::AddRef()
{
	++ref;
}

unsigned int D3DBlob::Release()
{
	if (--ref == 0)
	{
		delete this;
		return 0;
	}
	return ref;
}

unsigned int D3DBlob::Size()
{
	return size;
}

u8* D3DBlob::Data()
{
	return data;
}

}
