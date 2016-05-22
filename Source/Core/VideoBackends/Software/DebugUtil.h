// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Common.h"

namespace DebugUtil
{
	void Init();
	void Shutdown();

	void GetTextureRGBA(u8 *dst, u32 texmap, s32 mip, u32 width, u32 height);

	void DumpActiveTextures();

	void OnObjectBegin();
	void OnObjectEnd();

	void DrawObjectBuffer(s16 x, s16 y, u8 *color, int bufferBase, int subBuffer, const char *name);

	void DrawTempBuffer(u8 *color, int buffer);
	void CopyTempBuffer(s16 x, s16 y, int bufferBase, int subBuffer, const char *name);
}
