// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cassert>

#include <QFont>

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
}

void ConfigSlider::Update(int value)
{
  if (!m_tick_values.empty())
  {
    if (value >= 0 && static_cast<size_t>(value) < m_tick_values.size())
      SaveValue(m_setting, m_tick_values[static_cast<size_t>(value)]);
  }
  else
  {
    SaveValue(m_setting, value);
  }
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

ConfigSliderLabel::ConfigSliderLabel(const QString& text, ConfigSlider* slider)
    : QLabel(text), m_slider(QPointer<ConfigSlider>(slider))
{
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this]() {
    // Label shares font changes with slider to mark game ini settings.
    if (m_slider)
      setFont(m_slider->font());
  });
}
