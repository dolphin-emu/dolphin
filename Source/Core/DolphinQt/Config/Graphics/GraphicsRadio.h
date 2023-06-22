// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/ToolTipControls/ToolTipRadioButton.h"

#include "Common/Config/Config.h"

class GraphicsRadioInt : public ToolTipRadioButton
{
  Q_OBJECT
public:
  GraphicsRadioInt(const QString& label, const Config::Info<int>& setting, int value);

private:
  void Update();

  Config::Info<int> m_setting;
  int m_value;
};
