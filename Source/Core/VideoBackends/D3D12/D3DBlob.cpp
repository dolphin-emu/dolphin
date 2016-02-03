// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <d3d11.h>

#include "VideoBackends/D3D12/D3DBlob.h"

namespace DX12
{

D3DBlob::D3DBlob(unsigned int blob_size, const u8* init_data) : m_size(blob_size)
{
	m_data = new u8[blob_size];
	if (init_data)
		memcpy(m_data, init_data, m_size);
}

D3DBlob::D3DBlob(ID3D10Blob* d3dblob) : m_ref(1)
{
	m_blob = d3dblob;
	m_data = static_cast<u8*>(m_blob->GetBufferPointer());
	m_size = static_cast<unsigned int>(m_blob->GetBufferSize());
	d3dblob->AddRef();
}

D3DBlob::~D3DBlob()
{
	if (m_blob)
		m_blob->Release();
	else
		delete[] m_data;
}

void D3DBlob::AddRef()
{
	InterlockedIncrement(&m_ref);
}

unsigned int D3DBlob::Release()
{
	if (InterlockedDecrement(&m_ref) == 0)
	{
		delete this;
		return 0;
	}
	return m_ref;
}

unsigned int D3DBlob::Size() const
{
	return m_size;
}

u8* D3DBlob::Data() const
{
	return m_data;
}

}  // namespace DX12
