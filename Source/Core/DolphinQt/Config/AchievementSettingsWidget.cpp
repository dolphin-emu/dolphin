// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// 15 JAN 2023 - Lilly Jade Katrin - lilly.kitty.1988@gmail.com
// Thanks to Stenzek and the PCSX2 project for inspiration, assistance and examples,
// to TheFetishMachine and Infernum for encouragement and cheerleading,
// and to Gollawiz, Sea, Fridge, jenette and Ryudo for testing

#include "DolphinQt/Config/AchievementSettingsWidget.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

#include "Core/AchievementManager.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"

#include <Core/Config/AchievementSettings.h>
#include <ModalMessageBox.h>
#include "DolphinQt/Config/AchievementsWindow.h"
#include "DolphinQt/Config/ControllerInterface/ControllerInterfaceWindow.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

AchievementSettingsWidget::AchievementSettingsWidget(QWidget* parent,
                                                     AchievementsWindow* parent_window)
    : QWidget(parent), parent_window(parent_window)
{
  CreateLayout();
  LoadSettings();
  ConnectWidgets();

  connect(&Settings::Instance(), &Settings::ConfigChanged, this,
          &AchievementSettingsWidget::LoadSettings);
}

void AchievementSettingsWidget::CreateLayout()
{
  m_common_box = new QGroupBox();
  m_common_layout = new QVBoxLayout();
  m_common_integration_enabled_input = new QCheckBox(tr("Enable RetroAchievements Integration"));
  m_common_login_failed = new QLabel(tr("Login Failed"));
  m_common_login_failed->setStyleSheet(QStringLiteral("QLabel { color : red; }"));
  m_common_login_failed->setVisible(false);
  m_common_username_label = new QLabel(tr("Username"));
  m_common_username_input = new QLineEdit(tr("Username"));
  m_common_password_label = new QLabel(tr("Password"));
  m_common_password_input = new QLineEdit(QStringLiteral(""));
  m_common_password_input->setEchoMode(QLineEdit::Password);
  m_common_login_button = new QPushButton(tr("Login"));
  m_common_logout_button = new QPushButton(tr("Logout"));
  m_common_achievements_enabled_input = new QCheckBox(tr("Enable Achievements"));
  m_common_leaderboards_enabled_input = new QCheckBox(tr("Enable Leaderboards"));
  m_common_rich_presence_enabled_input = new QCheckBox(tr("Enable Rich Presence"));
  m_common_hardcore_enabled_input = new QCheckBox(tr("Enable Hardcore Mode"));
  m_common_badge_icons_enabled_input = new QCheckBox(tr("Enable Badge Icons"));
  m_common_unofficial_enabled_input = new QCheckBox(tr("Enable Unofficial Achievements"));
  m_common_encore_enabled_input = new QCheckBox(tr("Enable Encore Achievements"));

  m_common_layout->addWidget(m_common_integration_enabled_input);
  m_common_layout->addWidget(m_common_login_failed);
  m_common_layout->addWidget(m_common_username_label);
  m_common_layout->addWidget(m_common_username_input);
  m_common_layout->addWidget(m_common_password_label);
  m_common_layout->addWidget(m_common_password_input);
  m_common_layout->addWidget(m_common_login_button);
  m_common_layout->addWidget(m_common_logout_button);
  m_common_layout->addWidget(m_common_achievements_enabled_input);
  m_common_layout->addWidget(m_common_leaderboards_enabled_input);
  m_common_layout->addWidget(m_common_rich_presence_enabled_input);
  m_common_layout->addWidget(m_common_hardcore_enabled_input);
  m_common_layout->addWidget(m_common_badge_icons_enabled_input);
  m_common_layout->addWidget(m_common_unofficial_enabled_input);
  m_common_layout->addWidget(m_common_encore_enabled_input);

  m_common_box->setLayout(m_common_layout);

  auto* layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setAlignment(Qt::AlignTop);
  layout->addWidget(m_common_box);
  setLayout(layout);
}

void AchievementSettingsWidget::ConnectWidgets()
{
  connect(m_common_integration_enabled_input, &QCheckBox::toggled, this,
          &AchievementSettingsWidget::ToggleRAIntegration);
  connect(m_common_login_button, &QPushButton::pressed, this, &AchievementSettingsWidget::Login);
  connect(m_common_logout_button, &QPushButton::pressed, this, &AchievementSettingsWidget::Logout);
  connect(m_common_achievements_enabled_input, &QCheckBox::toggled, this,
          &AchievementSettingsWidget::ToggleAchievements);
  connect(m_common_leaderboards_enabled_input, &QCheckBox::toggled, this,
          &AchievementSettingsWidget::ToggleLeaderboards);
  connect(m_common_rich_presence_enabled_input, &QCheckBox::toggled, this,
          &AchievementSettingsWidget::ToggleRichPresence);
  connect(m_common_hardcore_enabled_input, &QCheckBox::clicked, this,
          &AchievementSettingsWidget::ToggleHardcore);
  connect(m_common_badge_icons_enabled_input, &QCheckBox::toggled, this,
          &AchievementSettingsWidget::ToggleBadgeIcons);
  connect(m_common_unofficial_enabled_input, &QCheckBox::toggled, this,
          &AchievementSettingsWidget::ToggleUnofficial);
  connect(m_common_encore_enabled_input, &QCheckBox::toggled, this,
          &AchievementSettingsWidget::ToggleEncore);
}

void AchievementSettingsWidget::OnControllerInterfaceConfigure()
{
  ControllerInterfaceWindow* window = new ControllerInterfaceWindow(this);
  window->setAttribute(Qt::WA_DeleteOnClose, true);
  window->setWindowModality(Qt::WindowModality::WindowModal);
  window->show();
}

void AchievementSettingsWidget::LoadSettings()
{
  SignalBlocking(m_common_integration_enabled_input)
      ->setChecked(Config::Get(Config::RA_INTEGRATION_ENABLED));

  SignalBlocking(m_common_username_label)->setEnabled(Config::Get(Config::RA_INTEGRATION_ENABLED));
  if (!Config::Get(Config::RA_USERNAME).empty())
    SignalBlocking(m_common_username_input)
        ->setText(QString::fromStdString(Config::Get(Config::RA_USERNAME)));
  SignalBlocking(m_common_username_input)
      ->setEnabled(Config::Get(Config::RA_INTEGRATION_ENABLED) &&
                   Config::Get(Config::RA_API_TOKEN).empty());
  SignalBlocking(m_common_password_label)->setVisible(Config::Get(Config::RA_API_TOKEN).empty());
  SignalBlocking(m_common_password_label)->setEnabled(Config::Get(Config::RA_INTEGRATION_ENABLED));
  SignalBlocking(m_common_password_input)->setVisible(Config::Get(Config::RA_API_TOKEN).empty());
  SignalBlocking(m_common_password_input)->setEnabled(Config::Get(Config::RA_INTEGRATION_ENABLED));
  SignalBlocking(m_common_login_button)->setVisible(Config::Get(Config::RA_API_TOKEN).empty());
  SignalBlocking(m_common_login_button)->setEnabled(Config::Get(Config::RA_INTEGRATION_ENABLED));
  SignalBlocking(m_common_logout_button)->setVisible(!Config::Get(Config::RA_API_TOKEN).empty());
  SignalBlocking(m_common_logout_button)->setEnabled(Config::Get(Config::RA_INTEGRATION_ENABLED));

  SignalBlocking(m_common_achievements_enabled_input)
      ->setChecked(Config::Get(Config::RA_ACHIEVEMENTS_ENABLED));
  SignalBlocking(m_common_achievements_enabled_input)
      ->setEnabled(Config::Get(Config::RA_INTEGRATION_ENABLED));

  SignalBlocking(m_common_leaderboards_enabled_input)
      ->setChecked(Config::Get(Config::RA_LEADERBOARDS_ENABLED));
  SignalBlocking(m_common_leaderboards_enabled_input)
      ->setEnabled(Config::Get(Config::RA_INTEGRATION_ENABLED) &&
                   Config::Get(Config::RA_HARDCORE_ENABLED));

  SignalBlocking(m_common_rich_presence_enabled_input)
      ->setChecked(Config::Get(Config::RA_RICH_PRESENCE_ENABLED));
  SignalBlocking(m_common_rich_presence_enabled_input)
      ->setEnabled(Config::Get(Config::RA_INTEGRATION_ENABLED));

  SignalBlocking(m_common_hardcore_enabled_input)
      ->setChecked(Config::Get(Config::RA_HARDCORE_ENABLED));
  SignalBlocking(m_common_hardcore_enabled_input)
      ->setEnabled(Config::Get(Config::RA_INTEGRATION_ENABLED));
  Settings::Instance().SetHardcoreModeEnabled(m_common_hardcore_enabled_input->isChecked());

  SignalBlocking(m_common_badge_icons_enabled_input)
      ->setChecked(Config::Get(Config::RA_BADGE_ICONS_ENABLED));
  SignalBlocking(m_common_badge_icons_enabled_input)
      ->setEnabled(Config::Get(Config::RA_INTEGRATION_ENABLED));

  SignalBlocking(m_common_unofficial_enabled_input)
      ->setChecked(Config::Get(Config::RA_UNOFFICIAL_ENABLED));
  SignalBlocking(m_common_unofficial_enabled_input)
      ->setEnabled(Config::Get(Config::RA_INTEGRATION_ENABLED) &&
                   Config::Get(Config::RA_ACHIEVEMENTS_ENABLED));

  SignalBlocking(m_common_encore_enabled_input)->setChecked(Config::Get(Config::RA_ENCORE_ENABLED));
  SignalBlocking(m_common_encore_enabled_input)
      ->setEnabled(Config::Get(Config::RA_INTEGRATION_ENABLED) &&
                   Config::Get(Config::RA_ACHIEVEMENTS_ENABLED));
}

void AchievementSettingsWidget::SaveSettings()
{
  Config::SetBaseOrCurrent(Config::RA_INTEGRATION_ENABLED,
                           m_common_integration_enabled_input->isChecked());
  Config::SetBaseOrCurrent(Config::RA_ACHIEVEMENTS_ENABLED,
                           m_common_achievements_enabled_input->isChecked());
  Config::SetBaseOrCurrent(Config::RA_LEADERBOARDS_ENABLED,
                           m_common_leaderboards_enabled_input->isChecked());
  Config::SetBaseOrCurrent(Config::RA_RICH_PRESENCE_ENABLED,
                           m_common_rich_presence_enabled_input->isChecked());
  Settings::Instance().SetHardcoreModeEnabled(m_common_hardcore_enabled_input->isChecked());
  Config::SetBaseOrCurrent(Config::RA_BADGE_ICONS_ENABLED,
                           m_common_badge_icons_enabled_input->isChecked());
  Config::SetBaseOrCurrent(Config::RA_UNOFFICIAL_ENABLED,
                           m_common_unofficial_enabled_input->isChecked());
  Config::SetBaseOrCurrent(Config::RA_ENCORE_ENABLED, m_common_encore_enabled_input->isChecked());
  Config::Save();
  parent_window->UpdateData();
}

void AchievementSettingsWidget::ToggleRAIntegration()
{
  if (Config::Get(Config::RA_INTEGRATION_ENABLED))
    Achievements::Init();
  else
  {
    Config::SetBaseOrCurrent(Config::RA_HARDCORE_ENABLED, false);
    Achievements::Shutdown();
  }
  SaveSettings();
}

void AchievementSettingsWidget::Login()
{
  Config::SetBaseOrCurrent(Config::RA_USERNAME, m_common_username_input->text().toStdString());
  Config::SetBaseOrCurrent(Config::RA_API_TOKEN,
                           Achievements::Login(m_common_password_input->text().toStdString()));
  Config::Save();
  m_common_password_input->setText(QString());
  m_common_login_failed->setVisible(Config::Get(Config::RA_API_TOKEN).empty());
  SaveSettings();
}

void AchievementSettingsWidget::Logout()
{
  Achievements::Logout();
  Config::SetBaseOrCurrent(Config::RA_API_TOKEN, "");
  Config::Save();
  SaveSettings();
}

void AchievementSettingsWidget::ToggleAchievements()
{
  if (Config::Get(Config::RA_ACHIEVEMENTS_ENABLED))
    Achievements::ActivateAM();
  else
    Achievements::DeactivateAM();
  SaveSettings();
}

void AchievementSettingsWidget::ToggleLeaderboards()
{
  if (Config::Get(Config::RA_LEADERBOARDS_ENABLED))
    Achievements::ActivateLB();
  else
    Achievements::DeactivateLB();
  SaveSettings();
}

void AchievementSettingsWidget::ToggleRichPresence()
{
  if (Config::Get(Config::RA_RICH_PRESENCE_ENABLED))
    Achievements::ActivateRP();
  else
    Achievements::DeactivateRP();
  SaveSettings();
}

void AchievementSettingsWidget::ToggleHardcore()
{
  if (!Config::Get(Config::RA_HARDCORE_ENABLED))
  {
    auto confirm = ModalMessageBox::question(static_cast<QWidget*>(this), tr("Confirm"),
                                             tr("Hardcore mode doubles the number of points "
                                                "you earn for each achievement, enables "
                                                "leaderboard submissions, and is "
                                                "necessary for mastering games.\n"
                                                "HOWEVER, enabling Hardcore mode will "
                                                "disable loading savestates, cheats, "
                                                "debug mode, frame advance, and emulator "
                                                "speeds less than 1x, and will immediately "
                                                "reset your current game.\n"
                                                "Do you want to turn on Hardcore mode?"),
                                             QMessageBox::Yes | QMessageBox::No,
                                             QMessageBox::NoButton, Qt::ApplicationModal);
    if (confirm == QMessageBox::Yes)
      Settings::Instance().SetHardcoreModeEnabled(true);
    else
    {
      Config::SetBaseOrCurrent(Config::RA_HARDCORE_ENABLED, false);
      m_common_hardcore_enabled_input->setChecked(false);
    }
  }
  else
  {
    auto confirm = ModalMessageBox::question(static_cast<QWidget*>(this), tr("Confirm"),
                                             tr("Disabling Hardcore mode will re-enable "
                                                "loading savestates, cheats, debug mode, "
                                                "frame advance, and emulator speeds less "
                                                "than 1x.\nHOWEVER, it will disable "
                                                "leaderboard submissions and cannot be "
                                                "turned back on without resetting your "
                                                "game.\n"
                                                "Do you want to turn off Hardcore mode?"),
                                             QMessageBox::Yes | QMessageBox::No,
                                             QMessageBox::NoButton, Qt::ApplicationModal);
    if (confirm == QMessageBox::Yes)
    {
      Settings::Instance().SetHardcoreModeEnabled(false);
      // Only really need to do this on hardcore disabled to disable softcore achievements
      // because on hardcore enabled there should be a full game data delete and reload
      Achievements::ActivateAM();
    }
    else
    {
      Config::SetBaseOrCurrent(Config::RA_HARDCORE_ENABLED, true);
      m_common_hardcore_enabled_input->setChecked(true);
    }
  }
  SaveSettings();
}

void AchievementSettingsWidget::ToggleBadgeIcons()
{
  if (Config::Get(Config::RA_BADGE_ICONS_ENABLED))
    Achievements::FetchData();
  // Because it takes a substantial amount of time and bandwidth to
  // reload all those badge icons, Dolphin will retain them in memory
  // as long as the game session is active, and will take no immediate
  // action if they are disabled.
  SaveSettings();
}

void AchievementSettingsWidget::ToggleUnofficial()
{
  Achievements::ActivateAM();
  SaveSettings();
}

void AchievementSettingsWidget::ToggleEncore()
{
  Achievements::ActivateAM();
  SaveSettings();
}
