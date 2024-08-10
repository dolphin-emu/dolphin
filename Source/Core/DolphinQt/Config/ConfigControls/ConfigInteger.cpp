// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ConfigControls/ConfigInteger.h"

#include <QSignalBlocker>

#include "Common/Config/Config.h"

#include "DolphinQt/Settings.h"

ConfigInteger::ConfigInteger(const int minimum, const int maximum, const Config::Info<int>& setting,
                             const int step)
    : ToolTipSpinBox(), m_setting(setting)
{
  setMinimum(minimum);
  setMaximum(maximum);
  setSingleStep(step);

  setValue(Get(setting));

  connect(this, &ConfigInteger::valueChanged, this, &ConfigInteger::Update);
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this] {
    QFont bf = font();
    bf.setBold(GetActiveLayerForConfig(m_setting) != Config::LayerType::Base);
    setFont(bf);

    const QSignalBlocker blocker(this);
    setValue(Get(m_setting));
  });
}

void ConfigInteger::Update(const int value) const
{
  SetBaseOrCurrent(m_setting, value);
}
