// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/ToolTipControls/ToolTipDoubleSpinBox.h"

namespace Config
{
template <typename T>
class Info;
}

class GraphicsDouble : public ToolTipDoubleSpinBox
{
  Q_OBJECT
public:
  GraphicsDouble(double minimum, double maximum, const Config::Info<double>& setting,
                 double step = 1.0, int decimals = 1);
  void Update(double value);

private:
  const Config::Info<double>& m_setting;
};
