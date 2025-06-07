// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ConfigControls/ConfigFloatSlider.h"
#include "QTimer"

ConfigFloatSlider::ConfigFloatSlider(float minimum, float maximum,
                                     const Config::Info<float>& setting, float step,
                                     Config::Layer* layer)
    : ConfigControl(Qt::Horizontal, setting.GetLocation(), layer), m_minimum(minimum), m_step(step),
      m_setting(setting)
{
  const float range = maximum - minimum;
  const int steps = std::round(range / step);
  const int interval = std::round(range / steps);
  const int current_value = std::round((ReadValue(setting) - minimum) / step);

  setMinimum(0);
  setMaximum(steps);
  setTickInterval(interval);
  setValue(current_value);

  connect(this, &ConfigFloatSlider::valueChanged, this, &ConfigFloatSlider::Update);

  m_timer = new QTimer(this);
  m_timer->setSingleShot(true);
  connect(m_timer, &QTimer::timeout, this, [this]() {
    if (m_last_value != this->value())
      Update(this->value());
  });
}

void ConfigFloatSlider::Update(int value)
{
  if (m_timer->isActive())
    return;

  m_last_value = value;
  const float current_value = (m_step * value) + m_minimum;

  SaveValue(m_setting, current_value);

  m_timer->start(100);
}

float ConfigFloatSlider::GetValue() const
{
  return (m_step * value()) + m_minimum;
}

void ConfigFloatSlider::OnConfigChanged()
{
  setValue(std::round((ReadValue(m_setting) - m_minimum) / m_step));
}
