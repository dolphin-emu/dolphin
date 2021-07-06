// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace ciface::Win32
{
void Init(void* hwnd);
void PopulateDevices(void* hwnd);
void ChangeWindow(void* hwnd);
void DeInit();
}  // namespace ciface::Win32
