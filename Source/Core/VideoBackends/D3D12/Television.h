// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/VideoCommon.h"

// D3D12TODO: Add DX12 path for this file.

namespace DX12
{

class Television final
{

public:

	Television();

	void Init();
	void Shutdown();

	// Submit video data to be drawn. This will change the current state of the
	// TV. xfbAddr points to YUYV data stored in GameCube/Wii RAM, but the XFB
	// may be virtualized when rendering so the RAM may not actually be read.
	void Submit(u32 xfb_address, u32 stride, u32 width, u32 height);

	// Render the current state of the TV.
	void Render();

private:


};

}
