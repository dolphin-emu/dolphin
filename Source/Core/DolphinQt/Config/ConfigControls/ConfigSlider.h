// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

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

  void Update(int value);

protected:
  void OnConfigChanged() override;

private:
  const Config::Info<int> m_setting;
};

class ConfigSliderLabel final : public QLabel
{
  Q_OBJECT

public:
  ConfigSliderLabel(const QString& text, ConfigSlider* slider);

private:
  QPointer<ConfigSlider> m_slider;
};
