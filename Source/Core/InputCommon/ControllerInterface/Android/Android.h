// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace ciface::Android
{
void SetMotionSensorsEnabled(bool accelerometer_enabled, bool gyroscope_enabled);

void PopulateDevices();
}  // namespace ciface::Android
