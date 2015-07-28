// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

struct ID3D10Blob;

namespace DX11
{

// use this class instead ID3D10Blob or ID3D11Blob whenever possible
class D3DBlob
{
public:
	// memory will be copied into an own buffer
	D3DBlob(unsigned int blob_size, const u8* init_data = nullptr);

	// d3dblob will be AddRef'd
	D3DBlob(ID3D10Blob* d3dblob);

	void AddRef();
	unsigned int Release();

	unsigned int Size() const;
	u8* Data();

private:
	~D3DBlob();

	unsigned int ref;
	unsigned int size;

	u8* data;
	ID3D10Blob* blob;
};

}  // namespace
