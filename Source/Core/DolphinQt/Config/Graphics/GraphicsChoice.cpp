// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Graphics/GraphicsChoice.h"

#include <QSignalBlocker>

#include "Common/Config/Config.h"

#include "DolphinQt/Settings.h"

GraphicsChoice::GraphicsChoice(const QStringList& options, const Config::ConfigInfo<int>& setting)
    : m_setting(setting)
{
  addItems(options);
  connect(this, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
          &GraphicsChoice::Update);
  setCurrentIndex(Config::Get(m_setting));

  connect(&Settings::Instance(), &Settings::ConfigChanged, [this] {
    QFont bf = font();
    bf.setBold(Config::GetActiveLayerForConfig(m_setting) != Config::LayerType::Base);
    setFont(bf);

    const QSignalBlocker blocker(this);
    setCurrentIndex(Config::Get(m_setting));
  });
}

void GraphicsChoice::Update(int choice)
{
  Config::SetBaseOrCurrent(m_setting, choice);
}
