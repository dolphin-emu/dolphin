// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _RENDERER_H_
#define _RENDERER_H_

#include "CommonTypes.h"
#include "EfbInterface.h"

namespace SWRenderer
{
	void Init();
	void Prepare();
	void Shutdown();

	void RenderText(const char* pstr, int left, int top, u32 color);
	void DrawDebugText();

	void UpdateColorTexture(EfbInterface::yuv422_packed *xfb);
	void DrawTexture(u8 *texture, int width, int height);

	void Swap(u32 fbWidth, u32 fbHeight);
	void SwapBuffer();
}

#endif
