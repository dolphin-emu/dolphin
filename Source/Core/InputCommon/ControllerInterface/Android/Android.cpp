// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/Android/Android.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Touch/Touchscreen.h"

namespace ciface::Android
{
static bool s_accelerometer_enabled = false;
static bool s_gyroscope_enabled = false;

void SetMotionSensorsEnabled(bool accelerometer_enabled, bool gyroscope_enabled)
{
  const bool any_changes =
      s_accelerometer_enabled != accelerometer_enabled || s_gyroscope_enabled != gyroscope_enabled;

  s_accelerometer_enabled = accelerometer_enabled;
  s_gyroscope_enabled = gyroscope_enabled;

  if (any_changes)
    g_controller_interface.RefreshDevices();
}

void PopulateDevices()
{
  for (int i = 0; i < 8; ++i)
    g_controller_interface.AddDevice(std::make_shared<ciface::Touch::Touchscreen>(
        i, s_accelerometer_enabled, s_gyroscope_enabled));
}
}  // namespace ciface::Android
