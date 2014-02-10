// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

// Initializes the FPS counter.
void InitFPSCounter();

// Called when a frame is rendered. Returns the value to be displayed on
// screen as the FPS counter (updated every second).
int UpdateFPSCounter();
