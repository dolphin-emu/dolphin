// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _VIDEOSTATE_H
#define _VIDEOSTATE_H

#include "Common.h"
#include "ChunkFile.h"

void VideoCommon_DoState(PointerWrap &p);
void VideoCommon_RunLoop(bool enable);
void VideoCommon_Init();

#endif // _VIDEOSTATE_H
