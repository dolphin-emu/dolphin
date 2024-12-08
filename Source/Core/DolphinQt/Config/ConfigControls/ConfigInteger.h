// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QLabel>
#include <QPointer>

#include "DolphinQt/Config/ConfigControls/ConfigControl.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipSpinBox.h"

namespace Config
{
template <typename T>
class Info;
}

class ConfigInteger final : public ConfigControl<ToolTipSpinBox>
{
  Q_OBJECT
public:
  ConfigInteger(int minimum, int maximum, const Config::Info<int>& setting, int step = 1);
  ConfigInteger(int minimum, int maximum, const Config::Info<int>& setting, Config::Layer* layer,
                int step = 1);

  void Update(int value);

protected:
  void OnConfigChanged() override;

private:
  const Config::Info<int>& m_setting;
};

class ConfigIntegerLabel final : public QLabel
{
  Q_OBJECT

public:
  ConfigIntegerLabel(const QString& text, ConfigInteger* widget);

private:
  QPointer<ConfigInteger> m_widget;
};
