// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/ToolTipControls/ToolTipSpinBox.h"

namespace Config
{
template <typename T>
class Info;
}

class ConfigInteger : public ToolTipSpinBox
{
  Q_OBJECT
public:
  ConfigInteger(int minimum, int maximum, const Config::Info<int>& setting, int step = 1);
  void Update(int value);

private:
  const Config::Info<int>& m_setting;
};
