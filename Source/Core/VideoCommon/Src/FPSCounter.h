// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _FPS_COUNTER_H_
#define _FPS_COUNTER_H_

// Initializes the FPS counter.
void InitFPSCounter();

// Called when a frame is rendered. Returns the value to be displayed on
// screen as the FPS counter (updated every second).
int UpdateFPSCounter();

#endif // _FPS_COUNTER_H_