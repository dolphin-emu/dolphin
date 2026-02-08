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

#include "DolphinQt/QtUtils/QtUtils.h"
#include "DolphinQt/Settings.h"

CheatWarningWidget::CheatWarningWidget(const std::string& game_id, bool restart_required,
                                       QWidget* parent)
    : QWidget(parent), m_game_id(game_id), m_restart_required(restart_required)
{
  CreateWidgets();
  ConnectWidgets();

  connect(&Settings::Instance(), &Settings::EnableCheatsChanged, this,
          [this] { Update(Core::IsRunning(Core::System::GetInstance())); });
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    Update(state == Core::State::Running || state == Core::State::Paused);
  });

  Update(Core::IsRunning(Core::System::GetInstance()));
}

void CheatWarningWidget::CreateWidgets()
{
  m_text = new QLabel();
  m_config_button = new QPushButton(tr("Configure Dolphin"));

  m_config_button->setHidden(true);

  auto* const layout = new QHBoxLayout{this};

  layout->addWidget(QtUtils::CreateIconWarning(this, QStyle::SP_MessageBoxWarning, m_text));
  layout->addWidget(m_config_button);

  layout->setContentsMargins(0, 0, 0, 0);
}

void CheatWarningWidget::Update(bool running)
{
  bool hide_widget = true;
  bool hide_config_button = true;

  if (running && SConfig::GetInstance().GetGameID() == m_game_id && m_restart_required)
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
  connect(m_config_button, &QPushButton::clicked, this,
          &CheatWarningWidget::OpenCheatEnableSettings);
}
