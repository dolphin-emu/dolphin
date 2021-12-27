// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
  class PrimeHackModes : public ControlGroup
  {
  public:
    explicit PrimeHackModes(const std::string& name);

    int GetSelectedDevice() const;
    bool GetMouseSupported() const;
    void SetSelectedDevice(int val);

  private:
    SettingValue<int> m_selection_value;
    NumericSetting<int> m_selection_setting = {&m_selection_value, {"PrimeHackMode"}, 0, 0, 1};
  };
}  // namespace ControllerEmu
