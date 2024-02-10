// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/ToolTipControls/ToolTipSlider.h"

namespace Config
{
template <typename T>
class Info;
}

class ConfigSlider : public ToolTipSlider
{
  Q_OBJECT
public:
  ConfigSlider(int minimum, int maximum, const Config::Info<int>& setting, int tick = 0);
  void Update(int value);

private:
  const Config::Info<int>& m_setting;
};
