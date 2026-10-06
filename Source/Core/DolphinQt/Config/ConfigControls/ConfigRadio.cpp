// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ConfigControls/ConfigRadio.h"

ConfigRadioInt::ConfigRadioInt(const QString& label, const Config::Info<int>& setting, int value,
                               Config::Layer* layer)
    : ConfigControl(label, setting.GetLocation(), layer), m_setting(setting), m_value(value)
{
  setChecked(IsSelected());

  connect(this, &QRadioButton::toggled, this, &ConfigRadioInt::Update);
}

int ConfigRadioInt::GetValue() const
{
  return m_value;
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

bool ConfigRadioInt::IsSelected() const
{
  return ReadValue(m_setting) == m_value;
}

void ConfigRadioInt::OnConfigChanged()
{
  setChecked(IsSelected());
}

bool ConfigRadioInt::ShouldLabelBeBold() const
{
  return IsSelected() && ConfigControl::ShouldLabelBeBold();
}
