// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/CheatWarningWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>

#include <iostream>

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "DolphinQt2/Settings.h"

CheatWarningWidget::CheatWarningWidget(const std::string& game_id) : m_game_id(game_id)
{
  CreateWidgets();
  ConnectWidgets();

  connect(&Settings::Instance(), &Settings::EnableCheatsChanged,
          [this] { Update(Core::IsRunning()); });
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, [this](Core::State state) {
    std::cout << (state == Core::State::Running) << std::endl;
    Update(state == Core::State::Running);
  });

  Update(Core::IsRunning());
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

  if (running && SConfig::GetInstance().GetGameID() == m_game_id)
  {
    hide_widget = false;
    m_text->setText(tr("Changing cheats will only take effect when the game is restarted."));
  }

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
  connect(m_config_button, &QPushButton::pressed, this,
          &CheatWarningWidget::OpenCheatEnableSettings);
}
