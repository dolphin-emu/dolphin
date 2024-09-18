// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/ConfigControls/ConfigControl.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipSlider.h"

namespace Config
{
template <typename T>
class Info;
}

class ConfigSlider final : public ConfigControl<ToolTipSlider>
{
  Q_OBJECT
public:
  ConfigSlider(int minimum, int maximum, const Config::Info<int>& setting, int tick = 0);
  void Update(int value);

protected:
  void OnConfigChanged() override;

private:
  const Config::Info<int>& m_setting;
};
