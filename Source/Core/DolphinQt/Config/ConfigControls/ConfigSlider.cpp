// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ConfigControls/ConfigSlider.h"

#include <QSignalBlocker>

#include "Common/Config/Config.h"

#include "DolphinQt/Settings.h"

ConfigSlider::ConfigSlider(const int minimum, const int maximum, const Config::Info<int>& setting, const int tick)
    : ToolTipSlider(Qt::Horizontal), m_setting(setting)
{
  setMinimum(minimum);
  setMaximum(maximum);
  setTickInterval(tick);

  setValue(Get(setting));

  connect(this, &ConfigSlider::valueChanged, this, &ConfigSlider::Update);

  connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this] {
    QFont bf = font();
    bf.setBold(GetActiveLayerForConfig(m_setting) != Config::LayerType::Base);
    setFont(bf);

    const QSignalBlocker blocker(this);
    setValue(Get(m_setting));
  });
}

void ConfigSlider::Update(const int value) const
{
  SetBaseOrCurrent(m_setting, value);
}
