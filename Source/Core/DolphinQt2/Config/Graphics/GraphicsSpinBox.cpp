// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/Graphics/GraphicsSpinBox.h"

#include "Common/Config/Config.h"
#include "DolphinQt2/Settings.h"

GraphicsSpinBox::GraphicsSpinBox(int min, int max, const Config::ConfigInfo<int>& setting)
    : m_setting(setting)
{
  connect(this, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
          &GraphicsSpinBox::Update);
  setMinimum(min);
  setMaximum(max);

  setValue(Config::Get(m_setting));

  connect(&Settings::Instance(), &Settings::ConfigChanged, [this] {
    QFont bf = font();
    bf.setBold(Config::GetActiveLayerForConfig(m_setting) != Config::LayerType::Base);
    setFont(bf);
  });
}

void GraphicsSpinBox::Update(int value)
{
  Config::SetBaseOrCurrent(m_setting, value);
}
