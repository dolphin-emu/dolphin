// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/GraphicsBool.h"

#include <QEvent>
#include <QFont>
#include <QSignalBlocker>

#include "Common/Config/Config.h"

#include "DolphinQt/Config/Graphics/BalloonTip.h"
#include "DolphinQt/Settings.h"

GraphicsBool::GraphicsBool(const QString& label, const Config::Info<bool>& setting, bool reverse)
    : ToolTipCheckBox(label), m_setting(setting), m_reverse(reverse)
{
  connect(this, &QCheckBox::toggled, this, &GraphicsBool::Update);
  setChecked(Config::Get(m_setting) ^ reverse);

  connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this] {
    QFont bf = font();
    bf.setBold(Config::GetActiveLayerForConfig(m_setting) != Config::LayerType::Base);
    setFont(bf);

    const QSignalBlocker blocker(this);
    setChecked(Config::Get(m_setting) ^ m_reverse);
  });
}

void GraphicsBool::Update()
{
  Config::SetBaseOrCurrent(m_setting, static_cast<bool>(isChecked() ^ m_reverse));
}

GraphicsBoolEx::GraphicsBoolEx(const QString& label, const Config::Info<bool>& setting,
                               bool reverse)
    : ToolTipRadioButton(label), m_setting(setting), m_reverse(reverse)
{
  connect(this, &QCheckBox::toggled, this, &GraphicsBoolEx::Update);
  setChecked(Config::Get(m_setting) ^ reverse);

  if (Config::GetActiveLayerForConfig(m_setting) != Config::LayerType::Base)
  {
    QFont bf = font();
    bf.setBold(true);
    setFont(bf);
  }
}

void GraphicsBoolEx::Update()
{
  Config::SetBaseOrCurrent(m_setting, static_cast<bool>(isChecked() ^ m_reverse));
}
