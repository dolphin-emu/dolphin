// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/GraphicsSlider.h"

#include <QSignalBlocker>

#include "Common/Config/Config.h"

#include "DolphinQt/Settings.h"

GraphicsSlider::GraphicsSlider(int minimum, int maximum, const Config::Info<int>& setting, int tick)
    : ToolTipSlider(Qt::Horizontal), m_setting(setting)
{
  setMinimum(minimum);
  setMaximum(maximum);
  setTickInterval(tick);

  setValue(Config::Get(setting));

  connect(this, &GraphicsSlider::valueChanged, this, &GraphicsSlider::Update);

  connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this] {
    QFont bf = font();
    bf.setBold(Config::GetActiveLayerForConfig(m_setting) != Config::LayerType::Base);
    setFont(bf);

    const QSignalBlocker blocker(this);
    setValue(Config::Get(m_setting));
  });
}

void GraphicsSlider::Update(int value)
{
  Config::SetBaseOrCurrent(m_setting, value);
}
