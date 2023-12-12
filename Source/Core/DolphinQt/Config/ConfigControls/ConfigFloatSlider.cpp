// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ConfigControls/ConfigFloatSlider.h"

#include <QSignalBlocker>

#include "Common/Config/Config.h"

#include "DolphinQt/Settings.h"

ConfigFloatSlider::ConfigFloatSlider(float minimum, float maximum,
                                     const Config::Info<float>& setting, float step)
    : ToolTipSlider(Qt::Horizontal), m_minimum(minimum), m_step(step), m_setting(setting)
{
  const float range = maximum - minimum;
  const int steps = std::round(range / step);
  const int interval = std::round(range / steps);
  const int current_value = std::round((Config::Get(m_setting) - minimum) / step);

  setMinimum(0);
  setMaximum(steps);
  setTickInterval(interval);
  setValue(current_value);

  connect(this, &ConfigFloatSlider::valueChanged, this, &ConfigFloatSlider::Update);

  connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this] {
    QFont bf = font();
    bf.setBold(Config::GetActiveLayerForConfig(m_setting) != Config::LayerType::Base);
    setFont(bf);

    const QSignalBlocker blocker(this);
    const int value = std::round((Config::Get(m_setting) - m_minimum) / m_step);
    setValue(value);
  });
}

void ConfigFloatSlider::Update(int value)
{
  const float current_value = (m_step * value) + m_minimum;
  Config::SetBaseOrCurrent(m_setting, current_value);
}

float ConfigFloatSlider::GetValue() const
{
  return (m_step * value()) + m_minimum;
}
