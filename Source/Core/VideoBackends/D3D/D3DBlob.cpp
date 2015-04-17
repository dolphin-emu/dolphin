// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <d3d11.h>

#include "VideoBackends/D3D/D3DBlob.h"

namespace DX11
{

D3DBlob::D3DBlob(unsigned int blob_size, const u8* init_data) : ref(1), size(blob_size), blob(nullptr)
{
	data = new u8[blob_size];
	if (init_data) memcpy(data, init_data, size);
}

D3DBlob::D3DBlob(ID3D10Blob* d3dblob) : ref(1)
{
	blob = d3dblob;
	data = (u8*)blob->GetBufferPointer();
	size = (unsigned int)blob->GetBufferSize();
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

unsigned int D3DBlob::Size() const
{
	return size;
}

u8* D3DBlob::Data()
{
	return data;
}

}  // namespace DX11
