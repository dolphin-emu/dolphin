// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Graphics/GraphicsRadio.h"

#include <QSignalBlocker>

#include "Common/Config/Config.h"

#include "DolphinQt/Settings.h"

GraphicsRadioInt::GraphicsRadioInt(const QString& label, const Config::ConfigInfo<int>& setting,
                                   int value)
    : QRadioButton(label), m_setting(setting), m_value(value)
{
  setChecked(Config::Get(m_setting) == m_value);
  connect(this, &QRadioButton::toggled, this, &GraphicsRadioInt::Update);

  connect(&Settings::Instance(), &Settings::ConfigChanged, [this] {
    QFont bf = font();
    bf.setBold(Config::GetActiveLayerForConfig(m_setting) != Config::LayerType::Base);
    setFont(bf);

    const QSignalBlocker blocker(this);
    setChecked(Config::Get(m_setting) == m_value);
  });
}

void GraphicsRadioInt::Update()
{
  if (!isChecked())
    return;

  Config::SetBaseOrCurrent(m_setting, m_value);
}
