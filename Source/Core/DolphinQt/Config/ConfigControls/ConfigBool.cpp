// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"

ConfigBool::ConfigBool(const QString& label, const Config::Info<bool>& setting, bool reverse)
    : ConfigControl(label, setting.GetLocation()), m_setting(setting), m_reverse(reverse)
{
  setChecked(ReadValue(setting) ^ reverse);

  connect(this, &QCheckBox::toggled, this, &ConfigBool::Update);
}

void ConfigBool::Update()
{
  const bool value = static_cast<bool>(isChecked() ^ m_reverse);

  SaveValue(m_setting, value);
}

void ConfigBool::OnConfigChanged()
{
  setChecked(ReadValue(m_setting) ^ m_reverse);
}
