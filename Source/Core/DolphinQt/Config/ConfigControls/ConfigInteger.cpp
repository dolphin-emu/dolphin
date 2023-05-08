// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ConfigControls/ConfigInteger.h"

#include <QSignalBlocker>

#include "Common/Config/Config.h"

#include "DolphinQt/Settings.h"

ConfigInteger::ConfigInteger(int minimum, int maximum, const Config::Info<int>& setting, int step)
    : ToolTipSpinBox(), m_setting(setting)
{
  setMinimum(minimum);
  setMaximum(maximum);
  setSingleStep(step);

  setValue(Config::Get(setting));

  connect(this, qOverload<int>(&ConfigInteger::valueChanged), this, &ConfigInteger::Update);
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this] {
    QFont bf = font();
    bf.setBold(Config::GetActiveLayerForConfig(m_setting) != Config::LayerType::Base);
    setFont(bf);

    const QSignalBlocker blocker(this);
    setValue(Config::Get(m_setting));
  });
}

void ConfigInteger::Update(int value)
{
  Config::SetBaseOrCurrent(m_setting, value);
}
