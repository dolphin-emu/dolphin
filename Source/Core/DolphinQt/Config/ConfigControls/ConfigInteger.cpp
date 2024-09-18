// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ConfigControls/ConfigInteger.h"

ConfigInteger::ConfigInteger(int minimum, int maximum, const Config::Info<int>& setting, int step)
    : ConfigControl(setting.GetLocation()), m_setting(setting)
{
  setMinimum(minimum);
  setMaximum(maximum);
  setSingleStep(step);
  setValue(ReadValue(setting));

  connect(this, &ConfigInteger::valueChanged, this, &ConfigInteger::Update);
}

void ConfigInteger::Update(int value)
{
  SaveValue(m_setting, value);
}

void ConfigInteger::OnConfigChanged()
{
  setValue(ReadValue(m_setting));
}
