// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/ToolTipControls/ToolTipSlider.h"

namespace Config
{
template <typename T>
class Info;
}

// Automatically converts an int slider into a float one.
// Do not read the int values or ranges directly from it.
class ConfigFloatSlider : public ToolTipSlider
{
  Q_OBJECT
public:
  ConfigFloatSlider(float minimum, float maximum, const Config::Info<float>& setting, float step);
  void Update(int value);

  // Returns the adjusted float value
  float GetValue() const;

private:
  float m_minimum;
  float m_step;
  const Config::Info<float>& m_setting;
};
