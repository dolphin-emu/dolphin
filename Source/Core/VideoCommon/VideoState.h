// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/ChunkFile.h"
#include "Common/Common.h"

void VideoCommon_DoState(PointerWrap &p);
void VideoCommon_RunLoop(bool enable);
void VideoCommon_Init();
