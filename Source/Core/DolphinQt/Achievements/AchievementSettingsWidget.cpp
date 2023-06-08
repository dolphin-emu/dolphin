// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS
#include "DolphinQt/Achievements/AchievementSettingsWidget.h"

#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

#include "Core/AchievementManager.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/Config/FreeLookSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/Movie.h"

#include "DolphinQt/Config/ControllerInterface/ControllerInterfaceWindow.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipCheckBox.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

AchievementSettingsWidget::AchievementSettingsWidget(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  LoadSettings();
  ConnectWidgets();

  connect(&Settings::Instance(), &Settings::ConfigChanged, this,
          &AchievementSettingsWidget::LoadSettings);

  // If hardcore is enabled when the emulator starts, make sure it turns off what it needs to
  if (Config::Get(Config::RA_HARDCORE_ENABLED))
    ToggleHardcore();
}

void AchievementSettingsWidget::UpdateData()
{
  LoadSettings();
}

void AchievementSettingsWidget::CreateLayout()
{
  m_common_layout = new QVBoxLayout();
  m_common_integration_enabled_input =
      new ToolTipCheckBox(tr("Enable RetroAchievements.org Integration"));
  m_common_integration_enabled_input->SetDescription(
      tr("Enable integration with RetroAchievements for earning achievements and competing in "
         "leaderboards.<br><br>Must log in with a RetroAchievements account to use. Dolphin does "
         "not save your password locally and uses an API token to maintain login."));
  m_common_username_label = new QLabel(tr("Username"));
  m_common_username_input = new QLineEdit(QStringLiteral(""));
  m_common_password_label = new QLabel(tr("Password"));
  m_common_password_input = new QLineEdit(QStringLiteral(""));
  m_common_password_input->setEchoMode(QLineEdit::Password);
  m_common_login_button = new QPushButton(tr("Log In"));
  m_common_logout_button = new QPushButton(tr("Log Out"));
  m_common_login_failed = new QLabel(tr("Login Failed"));
  m_common_login_failed->setStyleSheet(QStringLiteral("QLabel { color : red; }"));
  m_common_login_failed->setVisible(false);
  m_common_achievements_enabled_input = new ToolTipCheckBox(tr("Enable Achievements"));
  m_common_achievements_enabled_input->SetDescription(tr("Enable unlocking achievements.<br>"));
  m_common_leaderboards_enabled_input = new ToolTipCheckBox(tr("Enable Leaderboards"));
  m_common_leaderboards_enabled_input->SetDescription(
      tr("Enable competing in RetroAchievements leaderboards.<br><br>Hardcore Mode must be enabled "
         "to use."));
  m_common_rich_presence_enabled_input = new ToolTipCheckBox(tr("Enable Rich Presence"));
  m_common_rich_presence_enabled_input->SetDescription(
      tr("Enable detailed rich presence on the RetroAchievements website.<br><br>This provides a "
         "detailed description of what the player is doing in game to the website. If this is "
         "disabled, the website will only report what game is being played.<br><br>This has no "
         "bearing on Discord rich presence."));
  m_common_unofficial_enabled_input = new ToolTipCheckBox(tr("Enable Unofficial Achievements"));
  m_common_unofficial_enabled_input->SetDescription(
      tr("Enable unlocking unofficial achievements as well as official "
         "achievements.<br><br>Unofficial achievements may be optional or unfinished achievements "
         "that have not been deemed official by RetroAchievements and may be useful for testing or "
         "simply for fun."));
  m_common_hardcore_enabled_input = new ToolTipCheckBox(tr("Enable Hardcore Mode"));
  m_common_hardcore_enabled_input->SetDescription(
      tr("Enable Hardcore Mode on RetroAchievements.<br><br>Hardcore Mode is intended to provide "
         "an experience as close to gaming on the original hardware as possible. RetroAchievements "
         "rankings are primarily oriented towards Hardcore points (Softcore points are tracked but "
         "not as heavily emphasized) and leaderboards require Hardcore Mode to be on.<br><br>To "
         "ensure this experience, the following features will be disabled, as they give emulator "
         "players an advantage over console players:<br>- Loading states<br>-- Saving states is "
         "allowed<br>- Emulator speeds below 100%<br>-- Frame advance is disabled<br>-- Turbo is "
         "allowed<br>- Cheats<br>- Memory patches<br>-- File patches are allowed<br>- Debug "
         "UI<br>- Freelook<br><br><dolphin_emphasis>This cannot be turned on while a game is "
         "playing.</dolphin_emphasis><br>Close your current game before enabling.<br>Be aware that "
         "turning Hardcore Mode off while a game is running requires the game to be closed before "
         "re-enabling."));
  m_common_progress_enabled_input = new ToolTipCheckBox(tr("Enable Progress Notifications"));
  m_common_progress_enabled_input->SetDescription(
      tr("Enable progress notifications on achievements.<br><br>Displays a brief popup message "
         "whenever the player makes progress on an achievement that tracks an accumulated value, "
         "such as 60 out of 120 stars."));
  m_common_badges_enabled_input = new ToolTipCheckBox(tr("Enable Achievement Badges"));
  m_common_badges_enabled_input->SetDescription(
      tr("Enable achievement badges.<br><br>Displays icons for the player, game, and achievements. "
         "Simple visual option, but will require a small amount of extra memory and time to "
         "download the images."));
  m_common_encore_enabled_input = new ToolTipCheckBox(tr("Enable Encore Achievements"));
  m_common_encore_enabled_input->SetDescription(tr(
      "Enable unlocking achievements in Encore Mode.<br><br>Encore Mode re-enables achievements "
      "the player has already unlocked on the site so that the player will be notified if they "
      "meet the unlock conditions again, useful for custom speedrun criteria or simply for fun."));

  m_common_layout->addWidget(m_common_integration_enabled_input);
  m_common_layout->addWidget(m_common_username_label);
  m_common_layout->addWidget(m_common_username_input);
  m_common_layout->addWidget(m_common_password_label);
  m_common_layout->addWidget(m_common_password_input);
  m_common_layout->addWidget(m_common_login_button);
  m_common_layout->addWidget(m_common_logout_button);
  m_common_layout->addWidget(m_common_login_failed);
  m_common_layout->addWidget(m_common_achievements_enabled_input);
  m_common_layout->addWidget(m_common_leaderboards_enabled_input);
  m_common_layout->addWidget(m_common_rich_presence_enabled_input);
  m_common_layout->addWidget(m_common_hardcore_enabled_input);
  m_common_layout->addWidget(m_common_progress_enabled_input);
  m_common_layout->addWidget(m_common_badges_enabled_input);
  m_common_layout->addWidget(m_common_unofficial_enabled_input);
  m_common_layout->addWidget(m_common_encore_enabled_input);

  m_common_layout->setAlignment(Qt::AlignTop);
  setLayout(m_common_layout);
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
  connect(m_common_hardcore_enabled_input, &QCheckBox::toggled, this,
          &AchievementSettingsWidget::ToggleHardcore);
  connect(m_common_progress_enabled_input, &QCheckBox::toggled, this,
          &AchievementSettingsWidget::ToggleProgress);
  connect(m_common_badges_enabled_input, &QCheckBox::toggled, this,
          &AchievementSettingsWidget::ToggleBadges);
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
  bool enabled = Config::Get(Config::RA_ENABLED);
  bool achievements_enabled = Config::Get(Config::RA_ACHIEVEMENTS_ENABLED);
  bool hardcore_enabled = Config::Get(Config::RA_HARDCORE_ENABLED);
  bool logged_out = Config::Get(Config::RA_API_TOKEN).empty();
  std::string username = Config::Get(Config::RA_USERNAME);

  SignalBlocking(m_common_integration_enabled_input)->setChecked(enabled);
  SignalBlocking(m_common_username_label)->setEnabled(enabled);
  if (!username.empty())
    SignalBlocking(m_common_username_input)->setText(QString::fromStdString(username));
  SignalBlocking(m_common_username_input)->setEnabled(enabled && logged_out);
  SignalBlocking(m_common_password_label)->setVisible(logged_out);
  SignalBlocking(m_common_password_label)->setEnabled(enabled);
  SignalBlocking(m_common_password_input)->setVisible(logged_out);
  SignalBlocking(m_common_password_input)->setEnabled(enabled);
  SignalBlocking(m_common_login_button)->setVisible(logged_out);
  SignalBlocking(m_common_login_button)->setEnabled(enabled && !Core::IsRunning());
  SignalBlocking(m_common_logout_button)->setVisible(!logged_out);
  SignalBlocking(m_common_logout_button)->setEnabled(enabled);

  SignalBlocking(m_common_achievements_enabled_input)->setChecked(achievements_enabled);
  SignalBlocking(m_common_achievements_enabled_input)->setEnabled(enabled);

  SignalBlocking(m_common_leaderboards_enabled_input)
      ->setChecked(Config::Get(Config::RA_LEADERBOARDS_ENABLED));
  SignalBlocking(m_common_leaderboards_enabled_input)->setEnabled(enabled && hardcore_enabled);

  SignalBlocking(m_common_rich_presence_enabled_input)
      ->setChecked(Config::Get(Config::RA_RICH_PRESENCE_ENABLED));
  SignalBlocking(m_common_rich_presence_enabled_input)->setEnabled(enabled);

  SignalBlocking(m_common_hardcore_enabled_input)
      ->setChecked(Config::Get(Config::RA_HARDCORE_ENABLED));
  SignalBlocking(m_common_hardcore_enabled_input)
      ->setEnabled(enabled &&
                   (hardcore_enabled ||
                    (Core::GetState() == Core::State::Uninitialized && !Movie::IsPlayingInput())));

  SignalBlocking(m_common_progress_enabled_input)
      ->setChecked(Config::Get(Config::RA_PROGRESS_ENABLED));
  SignalBlocking(m_common_progress_enabled_input)->setEnabled(enabled && achievements_enabled);

  SignalBlocking(m_common_badges_enabled_input)->setChecked(Config::Get(Config::RA_BADGES_ENABLED));
  SignalBlocking(m_common_badges_enabled_input)->setEnabled(enabled);

  SignalBlocking(m_common_unofficial_enabled_input)
      ->setChecked(Config::Get(Config::RA_UNOFFICIAL_ENABLED));
  SignalBlocking(m_common_unofficial_enabled_input)->setEnabled(enabled && achievements_enabled);

  SignalBlocking(m_common_encore_enabled_input)->setChecked(Config::Get(Config::RA_ENCORE_ENABLED));
  SignalBlocking(m_common_encore_enabled_input)->setEnabled(enabled && achievements_enabled);
}

void AchievementSettingsWidget::SaveSettings()
{
  Config::ConfigChangeCallbackGuard config_guard;

  Config::SetBaseOrCurrent(Config::RA_ENABLED, m_common_integration_enabled_input->isChecked());
  Config::SetBaseOrCurrent(Config::RA_ACHIEVEMENTS_ENABLED,
                           m_common_achievements_enabled_input->isChecked());
  Config::SetBaseOrCurrent(Config::RA_LEADERBOARDS_ENABLED,
                           m_common_leaderboards_enabled_input->isChecked());
  Config::SetBaseOrCurrent(Config::RA_RICH_PRESENCE_ENABLED,
                           m_common_rich_presence_enabled_input->isChecked());
  Config::SetBaseOrCurrent(Config::RA_HARDCORE_ENABLED,
                           m_common_hardcore_enabled_input->isChecked());
  Config::SetBaseOrCurrent(Config::RA_PROGRESS_ENABLED,
                           m_common_unofficial_enabled_input->isChecked());
  Config::SetBaseOrCurrent(Config::RA_BADGES_ENABLED, m_common_badges_enabled_input->isChecked());
  Config::SetBaseOrCurrent(Config::RA_UNOFFICIAL_ENABLED,
                           m_common_unofficial_enabled_input->isChecked());
  Config::SetBaseOrCurrent(Config::RA_ENCORE_ENABLED, m_common_encore_enabled_input->isChecked());
  Config::Save();
}

void AchievementSettingsWidget::ToggleRAIntegration()
{
  SaveSettings();
  if (Config::Get(Config::RA_ENABLED))
    AchievementManager::GetInstance()->Init();
  else
    AchievementManager::GetInstance()->Shutdown();
}

void AchievementSettingsWidget::Login()
{
  Config::SetBaseOrCurrent(Config::RA_USERNAME, m_common_username_input->text().toStdString());
  AchievementManager::GetInstance()->Login(m_common_password_input->text().toStdString());
  m_common_password_input->setText(QString());
  m_common_login_failed->setVisible(Config::Get(Config::RA_API_TOKEN).empty());
  SaveSettings();
}

void AchievementSettingsWidget::Logout()
{
  AchievementManager::GetInstance()->Logout();
  SaveSettings();
}

void AchievementSettingsWidget::ToggleAchievements()
{
  SaveSettings();
  AchievementManager::GetInstance()->ActivateDeactivateAchievements();
}

void AchievementSettingsWidget::ToggleLeaderboards()
{
  SaveSettings();
  AchievementManager::GetInstance()->ActivateDeactivateLeaderboards();
}

void AchievementSettingsWidget::ToggleRichPresence()
{
  SaveSettings();
  AchievementManager::GetInstance()->ActivateDeactivateRichPresence();
}

void AchievementSettingsWidget::ToggleHardcore()
{
  SaveSettings();
  if (Config::Get(Config::RA_HARDCORE_ENABLED))
  {
    if (Config::Get(Config::MAIN_EMULATION_SPEED) < 1.0f)
      Config::SetBaseOrCurrent(Config::MAIN_EMULATION_SPEED, 1.0f);
    Config::SetBaseOrCurrent(Config::FREE_LOOK_ENABLED, false);
    Settings::Instance().SetCheatsEnabled(false);
    Settings::Instance().SetDebugModeEnabled(false);
  }
  emit Settings::Instance().EmulationStateChanged(Core::GetState());
}

void AchievementSettingsWidget::ToggleProgress()
{
  SaveSettings();
}

void AchievementSettingsWidget::ToggleBadges()
{
  SaveSettings();
  AchievementManager::GetInstance()->FetchBadges();
}

void AchievementSettingsWidget::ToggleUnofficial()
{
  SaveSettings();
  AchievementManager::GetInstance()->ActivateDeactivateAchievements();
}

void AchievementSettingsWidget::ToggleEncore()
{
  SaveSettings();
  AchievementManager::GetInstance()->ActivateDeactivateAchievements();
}

#endif  // USE_RETRO_ACHIEVEMENTS
