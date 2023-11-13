// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ConfigControls/ConfigRadio.h"

#include <QSignalBlocker>

#include "Common/Config/Config.h"

#include "DolphinQt/Settings.h"

ConfigRadioInt::ConfigRadioInt(const QString& label, const Config::Info<int>& setting, int value)
    : ToolTipRadioButton(label), m_setting(setting), m_value(value)
{
  setChecked(Config::Get(m_setting) == m_value);
  connect(this, &QRadioButton::toggled, this, &ConfigRadioInt::Update);

  connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this] {
    QFont bf = font();
    bf.setBold(Config::GetActiveLayerForConfig(m_setting) != Config::LayerType::Base);
    setFont(bf);

    const QSignalBlocker blocker(this);
    setChecked(Config::Get(m_setting) == m_value);
  });
}

void ConfigRadioInt::Update()
{
  if (isChecked())
  {
    Config::SetBaseOrCurrent(m_setting, m_value);
    emit OnSelected(m_value);
  }
  else
  {
    emit OnDeselected(m_value);
  }
}
