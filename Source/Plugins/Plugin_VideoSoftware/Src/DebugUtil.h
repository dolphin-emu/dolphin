// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _DEBUGUTIL_H
#define _DEBUGUTIL_H

namespace DebugUtil
{
	void Init();

	void GetTextureBGRA(u8 *dst, u32 texmap, s32 mip, u32 width, u32 height);

	void DumpActiveTextures();

	void OnObjectBegin();
	void OnObjectEnd();

	void OnFrameEnd();

	void DrawObjectBuffer(s16 x, s16 y, u8 *color, int bufferBase, int subBuffer, const char *name);

	void DrawTempBuffer(u8 *color, int buffer);
	void CopyTempBuffer(s16 x, s16 y, int bufferBase, int subBuffer, const char *name);
}

#endif 
