// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/Graphics/GraphicsRadio.h"

#include "Common/Config/Config.h"
#include "DolphinQt2/Settings.h"

GraphicsRadioInt::GraphicsRadioInt(const QString& label, const Config::ConfigInfo<int>& setting,
                                   int value)
    : QRadioButton(label), m_setting(setting), m_value(value)
{
  setChecked(Config::Get(m_setting) == m_value);
  connect(this, &QRadioButton::toggled, this, &GraphicsRadioInt::Update);
}

void GraphicsRadioInt::Update()
{
  if (!isChecked())
    return;

  Config::SetBaseOrCurrent(m_setting, m_value);
}
