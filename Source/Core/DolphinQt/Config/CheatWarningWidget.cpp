// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/CheatWarningWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/System.h"

#include "DolphinQt/Settings.h"

CheatWarningWidget::CheatWarningWidget(const std::string& game_id, QWidget* parent)
    : QWidget(parent), m_game_id(game_id)
{
  CreateWidgets();
  ConnectWidgets();

  connect(&Settings::Instance(), &Settings::EnableCheatsChanged, this,
          [this] { Update(Core::IsRunning(Core::System::GetInstance())); });
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          [this](Core::State state) { Update(state == Core::State::Running); });

  Update(Core::IsRunning(Core::System::GetInstance()));
}

void CheatWarningWidget::CreateWidgets()
{
  auto* icon = new QLabel;

  const auto size = 1.5 * QFontMetrics(font()).height();

  QPixmap warning_icon = style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(size, size);

  icon->setPixmap(warning_icon);

  m_text = new QLabel();
  m_config_button = new QPushButton(tr("Configure Dolphin"));

  m_config_button->setHidden(true);

  auto* layout = new QHBoxLayout;

  layout->addWidget(icon);
  layout->addWidget(m_text, 1);
  layout->addWidget(m_config_button);

  layout->setContentsMargins(0, 0, 0, 0);

  setLayout(layout);
}

void CheatWarningWidget::Update(bool running)
{
  bool hide_widget = true;
  bool hide_config_button = true;

  if (!Settings::Instance().GetCheatsEnabled())
  {
    hide_widget = false;
    hide_config_button = false;
    m_text->setText(tr("Dolphin's cheat system is currently disabled."));
  }

  setHidden(hide_widget);
  m_config_button->setHidden(hide_config_button);
}

void CheatWarningWidget::ConnectWidgets()
{
  connect(m_config_button, &QPushButton::clicked, this,
          &CheatWarningWidget::OpenCheatEnableSettings);
}
