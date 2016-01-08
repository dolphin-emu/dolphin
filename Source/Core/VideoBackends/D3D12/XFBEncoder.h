// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/VideoCommon.h"

namespace DX12
{

class XFBEncoder
{

public:
	XFBEncoder();

	void Init();
	void Shutdown();

	void Encode(u8* dst, u32 width, u32 height, const EFBRectangle& src_rect, float gamma);

private:
	// D3D12TODO: Implement this class

};

}
