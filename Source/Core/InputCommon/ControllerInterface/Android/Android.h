// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace ciface::Android
{
void SetMotionSensorsEnabled(bool accelerometer_enabled, bool gyroscope_enabled);

void PopulateDevices();
}  // namespace ciface::Android
