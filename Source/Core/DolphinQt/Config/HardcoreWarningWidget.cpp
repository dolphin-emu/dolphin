// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS
#include "DolphinQt/Config/HardcoreWarningWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>

#include "Core/Config/AchievementSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "DolphinQt/Settings.h"

HardcoreWarningWidget::HardcoreWarningWidget(QWidget* parent) : QWidget(parent)
{
  CreateWidgets();
  ConnectWidgets();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this] { Update(); });

  Update();
}

void HardcoreWarningWidget::CreateWidgets()
{
  const auto size = 1.5 * QFontMetrics(font()).height();

  QPixmap warning_icon = style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(size, size);

  auto* icon = new QLabel;
  icon->setPixmap(warning_icon);

  m_text = new QLabel(tr("This feature is disabled in hardcore mode."));
  m_settings_button = new QPushButton(tr("Achievement Settings"));

  auto* layout = new QHBoxLayout;

  layout->addWidget(icon);
  layout->addWidget(m_text, 1);
  layout->addWidget(m_settings_button);

  layout->setContentsMargins(0, 0, 0, 0);

  setLayout(layout);
}

void HardcoreWarningWidget::ConnectWidgets()
{
  connect(m_settings_button, &QPushButton::clicked, this,
          &HardcoreWarningWidget::OpenAchievementSettings);
}

void HardcoreWarningWidget::Update()
{
  setHidden(!Config::Get(Config::RA_HARDCORE_ENABLED));
}
#endif  // USE_RETRO_ACHIEVEMENTS
