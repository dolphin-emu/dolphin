// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QLabel>

#include "DolphinQt/Config/ConfigControls/ConfigControl.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipSlider.h"

#include "Common/Config/ConfigInfo.h"

// Automatically converts an int slider into a float one.
// Do not read the int values or ranges directly from it.
class ConfigFloatSlider final : public ConfigControl<ToolTipSlider>
{
  Q_OBJECT
public:
  ConfigFloatSlider(float minimum, float maximum, const Config::Info<float>& setting, float step,
                    Config::Layer* layer = nullptr);
  void Update(int value);

  // Returns the adjusted float value
  float GetValue() const;

protected:
  void OnConfigChanged() override;

private:
  float m_minimum;
  float m_step;
  const Config::Info<float> m_setting;
};

class ConfigFloatLabel final : public QLabel
{
public:
  ConfigFloatLabel(const QString& text, ConfigFloatSlider* widget);
};
