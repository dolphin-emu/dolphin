// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/Graphics/GraphicsSlider.h"

#include "Common/Config/Config.h"

GraphicsSlider::GraphicsSlider(int minimum, int maximum, const Config::ConfigInfo<int>& setting,
                               int tick)
    : QSlider(Qt::Horizontal), m_setting(setting)
{
  setMinimum(minimum);
  setMaximum(maximum);
  setTickInterval(tick);

  setValue(Config::Get(setting));

  connect(this, &GraphicsSlider::valueChanged, this, &GraphicsSlider::Update);
}

void GraphicsSlider::Update(int value)
{
  Config::SetBaseOrCurrent(m_setting, value);
}
