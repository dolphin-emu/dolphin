// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/ToolTipControls/ToolTipRadioButton.h"

#include "Common/Config/Config.h"

class ConfigRadioInt : public ToolTipRadioButton
{
  Q_OBJECT
public:
  ConfigRadioInt(const QString& label, const Config::Info<int>& setting, int value);

signals:
  // Since selecting a new radio button deselects the old one, ::toggled will generate two signals.
  // These are convenience functions so you can receive only one signal if desired.
  void OnSelected(int new_value);
  void OnDeselected(int old_value);

private:
  void Update();

  Config::Info<int> m_setting;
  int m_value;
};
