// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/Graphics/GraphicsChoice.h"

#include "Common/Config/Config.h"

GraphicsChoice::GraphicsChoice(const QStringList& options, const Config::ConfigInfo<int>& setting)
    : m_setting(setting)
{
  addItems(options);
  connect(this, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
          &GraphicsChoice::Update);
  setCurrentIndex(Config::Get(m_setting));

  if (Config::GetActiveLayerForConfig(m_setting) != Config::LayerType::Base)
  {
    QFont bf = font();
    bf.setBold(true);
    setFont(bf);
  }
}

void GraphicsChoice::Update(int choice)
{
  Config::SetBaseOrCurrent(m_setting, choice);
}
