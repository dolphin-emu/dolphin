// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/GraphicsChoice.h"

#include <QSignalBlocker>

#include "Common/Config/Config.h"

#include "DolphinQt/Settings.h"

GraphicsChoice::GraphicsChoice(const QStringList& options, const Config::Info<int>& setting)
    : m_setting(setting)
{
  addItems(options);
  connect(this, qOverload<int>(&QComboBox::currentIndexChanged), this, &GraphicsChoice::Update);
  setCurrentIndex(Config::Get(m_setting));

  connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this] {
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
