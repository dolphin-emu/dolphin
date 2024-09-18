// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ConfigControls/ConfigRadio.h"

ConfigRadioInt::ConfigRadioInt(const QString& label, const Config::Info<int>& setting, int value)
    : ConfigControl(label, setting.GetLocation()), m_setting(setting), m_value(value)
{
  setChecked(ReadValue(setting) == value);

  connect(this, &QRadioButton::toggled, this, &ConfigRadioInt::Update);
}

void ConfigRadioInt::Update()
{
  if (isChecked())
  {
    SaveValue(m_setting, m_value);

    emit OnSelected(m_value);
  }
  else
  {
    emit OnDeselected(m_value);
  }
}

void ConfigRadioInt::OnConfigChanged()
{
  setChecked(ReadValue(m_setting) == m_value);
}
