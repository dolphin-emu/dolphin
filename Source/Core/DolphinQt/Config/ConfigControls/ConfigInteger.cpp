// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ConfigControls/ConfigInteger.h"

ConfigInteger::ConfigInteger(int minimum, int maximum, const Config::Info<int>& setting, int step)
    : ConfigInteger(minimum, maximum, setting, nullptr, step)
{
}

ConfigInteger::ConfigInteger(int minimum, int maximum, const Config::Info<int>& setting,
                             Config::Layer* layer, int step)
    : ConfigControl(setting.GetLocation(), layer), m_setting(setting)
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

ConfigIntegerLabel::ConfigIntegerLabel(const QString& text, ConfigInteger* widget)
    : QLabel(text), m_widget(QPointer<ConfigInteger>(widget))
{
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this] {
    // Label shares font changes with ConfigInteger to mark game ini settings.
    if (m_widget)
      setFont(m_widget->font());
  });
}
