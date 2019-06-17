// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace ciface::OSX
{
void Init(void* window);
void PopulateDevices(void* window);
void DeInit();

void DeviceElementDebugPrint(const void*, void*);
}  // namespace ciface::OSX
