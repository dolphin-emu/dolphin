// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/GraphicsDouble.h"

#include <QSignalBlocker>

#include "Common/Config/Config.h"

#include "DolphinQt/Settings.h"

GraphicsDouble::GraphicsDouble(double minimum, double maximum, const Config::Info<double>& setting,
                               double step, int decimals)
    : ToolTipDoubleSpinBox(), m_setting(setting)
{
  setMinimum(minimum);
  setMaximum(maximum);
  setSingleStep(step);
  setDecimals(decimals);

  setValue(Config::Get(setting));

  connect(this, qOverload<double>(&GraphicsDouble::valueChanged), this, &GraphicsDouble::Update);
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this] {
    QFont bf = font();
    bf.setBold(Config::GetActiveLayerForConfig(m_setting) != Config::LayerType::Base);
    setFont(bf);

    const QSignalBlocker blocker(this);
    setValue(Config::Get(m_setting));
  });
}

void GraphicsDouble::Update(double value)
{
  Config::SetBaseOrCurrent(m_setting, value);
}
