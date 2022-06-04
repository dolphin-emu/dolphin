// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// XInput suffers a similar issue as XAudio2. Since Win8, it is part of the OS.
// However, unlike XAudio2 they have not made the API incompatible - so we just
// compile against the latest version and fall back to dynamically loading the
// old DLL.

#pragma once

#include <windows.h>
#include <XInput.h>

#include "InputCommon/ControllerInterface/ControllerInterface.h"

#ifndef XINPUT_DEVSUBTYPE_FLIGHT_STICK
#error You are building this module against the wrong version of DirectX. You probably need to remove DXSDK_DIR from your include path and/or _WIN32_WINNT is wrong.
#endif

namespace ciface::XInput
{
void Init();
void PopulateDevices();
void DeInit();

class Device final : public Core::Device
{
public:
  Device(const XINPUT_CAPABILITIES& capabilities, u8 index);

  std::string GetName() const override;
  std::string GetSource() const override;
  std::optional<int> GetPreferredId() const override;
  int GetSortPriority() const override { return -2; }

  void UpdateInput() override;

  void UpdateMotors();

private:
  XINPUT_STATE m_state_in{};
  XINPUT_VIBRATION m_state_out{};
  ControlState m_battery_level{};
  const BYTE m_subtype;
  const u8 m_index;
};
}  // namespace ciface::XInput
