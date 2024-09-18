// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/ConfigControls/ConfigControl.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipRadioButton.h"

namespace Config
{
template <typename T>
class Info;
}

class ConfigRadioInt final : public ConfigControl<ToolTipRadioButton>
{
  Q_OBJECT
public:
  ConfigRadioInt(const QString& label, const Config::Info<int>& setting, int value);

signals:
  // Since selecting a new radio button deselects the old one, ::toggled will generate two signals.
  // These are convenience functions so you can receive only one signal if desired.
  void OnSelected(int new_value);
  void OnDeselected(int old_value);

protected:
  void OnConfigChanged() override;

private:
  void Update();

  const Config::Info<int>& m_setting;
  int m_value;
};
