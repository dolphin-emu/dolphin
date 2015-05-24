// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

class PointerWrap;

void VideoCommon_DoState(PointerWrap &p);
void VideoCommon_RunLoop(bool enable);
void VideoCommon_Init();
