// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace ciface::Win32
{
void Init(void* hwnd);
void PopulateDevices(void* hwnd);
void DeInit();
}  // namespace ciface::Win32
