// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/CheatWarningWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>
#include <QTimer>

#include "Core/ConfigManager.h"
#include "Core/Core.h"

CheatWarningWidget::CheatWarningWidget(const std::string& game_id) : m_game_id(game_id)
{
  CreateWidgets();
  ConnectWidgets();

  setHidden(true);

  connect(this, &CheatWarningWidget::CheatEnableToggled, this,
          &CheatWarningWidget::CheatEnableToggled);
  connect(this, &CheatWarningWidget::EmulationStarted, this,
          &CheatWarningWidget::CheatEnableToggled);
  connect(this, &CheatWarningWidget::EmulationStopped, this,
          &CheatWarningWidget::CheatEnableToggled);
}

void CheatWarningWidget::CreateWidgets()
{
  auto* icon = new QLabel;

  QPixmap warning_icon = style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(24, 24);

  icon->setPixmap(warning_icon);

  m_text = new QLabel();
  m_config_button = new QPushButton(tr("Configure Dolphin"));

  m_config_button->setHidden(true);

  auto* layout = new QHBoxLayout;

  layout->addWidget(icon);
  layout->addWidget(m_text);
  layout->addWidget(m_config_button);

  setLayout(layout);
}

void CheatWarningWidget::Update()
{
  bool cheats_enabled = SConfig::GetInstance().bEnableCheats;

  bool hide = true;

  if (Core::IsRunning() && SConfig::GetInstance().GetGameID() == m_game_id)
  {
    hide = false;
    m_text->setText(tr("Changing cheats will only take effect when the game is restarted."));
  }

  if (!cheats_enabled)
  {
    hide = false;
    m_text->setText(tr("Dolphin's cheat system is currently disabled."));
  }

  m_config_button->setHidden(hide);
}

void CheatWarningWidget::ConnectWidgets()
{
  connect(m_config_button, &QPushButton::pressed, [this] { emit OpenCheatEnableSettings(); });
}
