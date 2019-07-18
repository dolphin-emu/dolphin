// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Graphics/GraphicsInteger.h"

#include <QSignalBlocker>

#include "Common/Config/Config.h"

#include "DolphinQt/Settings.h"

GraphicsInteger::GraphicsInteger(int minimum, int maximum, const Config::ConfigInfo<int>& setting,
                                 int step)
    : QSpinBox(), m_setting(setting)
{
  setMinimum(minimum);
  setMaximum(maximum);
  setSingleStep(step);

  setValue(Config::Get(setting));

  connect(this, static_cast<void (GraphicsInteger::*)(int)>(&GraphicsInteger::valueChanged), this,
          &GraphicsInteger::Update);

  connect(&Settings::Instance(), &Settings::ConfigChanged, [this] {
    QFont bf = font();
    bf.setBold(Config::GetActiveLayerForConfig(m_setting) != Config::LayerType::Base);
    setFont(bf);

    const QSignalBlocker blocker(this);
    setValue(Config::Get(m_setting));
  });
}

void GraphicsInteger::Update(int value)
{
  Config::SetBaseOrCurrent(m_setting, value);
}
