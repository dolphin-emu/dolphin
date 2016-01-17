// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D12/BoundingBox.h"

#include "VideoCommon/VideoConfig.h"

// D3D12TODO: Support bounding box behavior.
namespace DX12
{

ID3D11UnorderedAccessView* BBox::GetUAV()
{
	// D3D12TODO: Implement this;
	return nullptr;
}

void BBox::Init()
{
	if (g_ActiveConfig.backend_info.bSupportsBBox)
	{
		// D3D12TODO: Implement this;
	}
}

void BBox::Shutdown()
{
	// D3D12TODO: Implement this;
}

void BBox::Set(int index, int value)
{
	// D3D12TODO: Implement this;
}

int BBox::Get(int index)
{
	// D3D12TODO: Implement this;
	return 0;
}

};
