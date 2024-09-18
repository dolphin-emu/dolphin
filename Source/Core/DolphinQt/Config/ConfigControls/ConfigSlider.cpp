// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ConfigControls/ConfigSlider.h"

ConfigSlider::ConfigSlider(int minimum, int maximum, const Config::Info<int>& setting, int tick)
    : ConfigSlider(minimum, maximum, setting, nullptr, tick)
{
}

ConfigSlider::ConfigSlider(int minimum, int maximum, const Config::Info<int>& setting,
                           Config::Layer* layer, int tick)
    : ConfigControl(Qt::Horizontal, setting.GetLocation(), layer), m_setting(setting)

{
  setMinimum(minimum);
  setMaximum(maximum);
  setTickInterval(tick);
  setValue(ReadValue(setting));

  connect(this, &ConfigSlider::valueChanged, this, &ConfigSlider::Update);
}

void ConfigSlider::Update(int value)
{
  SaveValue(m_setting, value);
}

void ConfigSlider::OnConfigChanged()
{
  setValue(ReadValue(m_setting));
}
