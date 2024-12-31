// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include <QLabel>
#include <QPointer>

#include "DolphinQt/Config/ConfigControls/ConfigControl.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipSlider.h"

#include "Common/Config/ConfigInfo.h"

class ConfigSlider final : public ConfigControl<ToolTipSlider>
{
  Q_OBJECT
public:
  ConfigSlider(int minimum, int maximum, const Config::Info<int>& setting, int tick = 0);
  ConfigSlider(int minimum, int maximum, const Config::Info<int>& setting, Config::Layer* layer,
               int tick = 0);

  // Generates a slider with tick_values.size() ticks. Each tick corresponds to the integer at that
  // index in the vector.
  ConfigSlider(std::vector<int> tick_values, const Config::Info<int>& setting,
               Config::Layer* layer);

  void Update(int value);

protected:
  void OnConfigChanged() override;

private:
  const Config::Info<int> m_setting;

  // Mappings for slider ticks to config values. Identity mapping is assumed if this is empty.
  std::vector<int> m_tick_values;
};

class ConfigSliderLabel final : public QLabel
{
  Q_OBJECT

public:
  ConfigSliderLabel(const QString& text, ConfigSlider* slider);

private:
  QPointer<ConfigSlider> m_slider;
};
