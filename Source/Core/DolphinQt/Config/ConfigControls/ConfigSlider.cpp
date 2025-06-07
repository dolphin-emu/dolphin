// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cassert>

#include <QFont>
#include <QTimer>

#include "DolphinQt/Config/ConfigControls/ConfigSlider.h"
#include "DolphinQt/Settings.h"

ConfigSlider::ConfigSlider(int minimum, int maximum, const Config::Info<int>& setting, int tick)
    : ConfigSlider(minimum, maximum, setting, nullptr, tick)
{
}

ConfigSlider::ConfigSlider(int minimum, int maximum, const Config::Info<int>& setting,
                           Config::Layer* layer, int tick)
    : ConfigControl(Qt::Horizontal, setting.GetLocation(), layer), m_setting(setting)

{
  setMinimum(minimum);
  setMaximum(maximum);
  setTickInterval(tick);
  setValue(ReadValue(setting));

  connect(this, &ConfigSlider::valueChanged, this, &ConfigSlider::Update);

  m_timer = new QTimer(this);
  m_timer->setSingleShot(true);
  connect(m_timer, &QTimer::timeout, this, [this]() {
    if (m_last_value != this->value())
      Update(this->value());
  });
}

ConfigSlider::ConfigSlider(std::vector<int> tick_values, const Config::Info<int>& setting,
                           Config::Layer* layer)
    : ConfigControl(Qt::Horizontal, setting.GetLocation(), layer), m_setting(setting),
      m_tick_values(std::move(tick_values))
{
  assert(!m_tick_values.empty());
  setMinimum(0);
  setMaximum(static_cast<int>(m_tick_values.size() - 1));
  setPageStep(1);
  setTickPosition(QSlider::TicksBelow);
  OnConfigChanged();

  connect(this, &ConfigSlider::valueChanged, this, &ConfigSlider::Update);

  m_timer = new QTimer(this);
  m_timer->setSingleShot(true);
  connect(m_timer, &QTimer::timeout, this, [this]() {
    if (m_last_value != this->value())
      Update(this->value());
  });
}

void ConfigSlider::Update(int value)
{
  if (m_timer->isActive())
    return;

  m_last_value = value;

  if (!m_tick_values.empty())
  {
    if (value >= 0 && static_cast<size_t>(value) < m_tick_values.size())
      SaveValue(m_setting, m_tick_values[static_cast<size_t>(value)]);
  }
  else
  {
    SaveValue(m_setting, value);
  }

  m_timer->start(100);
}

void ConfigSlider::OnConfigChanged()
{
  if (!m_tick_values.empty())
  {
    // re-enable in case it was disabled
    setEnabled(true);

    const int config_value = ReadValue(m_setting);
    for (size_t i = 0; i < m_tick_values.size(); ++i)
    {
      if (m_tick_values[i] == config_value)
      {
        setValue(static_cast<int>(i));
        return;
      }
    }

    // if we reach here than none of the options matched, disable the slider
    setEnabled(false);
  }
  else
  {
    setValue(ReadValue(m_setting));
  }
}

ConfigSliderU32::ConfigSliderU32(u32 minimum, u32 maximum, const Config::Info<u32>& setting,
                                 u32 scale)
    : ConfigSliderU32(minimum, maximum, setting, nullptr, scale)
{
}

ConfigSliderU32::ConfigSliderU32(u32 minimum, u32 maximum, const Config::Info<u32>& setting,
                                 Config::Layer* layer, u32 scale)
    : ConfigControl(Qt::Horizontal, setting.GetLocation(), layer), m_setting(setting),
      m_scale(scale)

{
  setMinimum(minimum);
  setMaximum(maximum);
  setValue(ReadValue(setting));
  OnConfigChanged();

  m_timer = new QTimer(this);
  connect(this, &ConfigSliderU32::valueChanged, this, &ConfigSliderU32::Update);
  connect(m_timer, &QTimer::timeout, this, [this]() {
    if (m_last_value != this->value())
      Update(this->value());
  });
}

void ConfigSliderU32::Update(int value)
{
  if (m_timer->isActive())
    return;

  m_last_value = value;
  SaveValue(m_setting, static_cast<u32>(value) * m_scale);

  m_timer->start(100);
}

void ConfigSliderU32::OnConfigChanged()
{
  setValue(ReadValue(m_setting) / m_scale);
}

ConfigSliderLabel::ConfigSliderLabel(const QString& text, ConfigSlider* slider)
    : QLabel(text), m_slider(QPointer<ConfigSlider>(slider))
{
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this]() {
    // Label shares font changes with slider to mark game ini settings.
    if (m_slider)
      setFont(m_slider->font());
  });
}
