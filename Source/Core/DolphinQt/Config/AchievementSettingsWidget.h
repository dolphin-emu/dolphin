// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// 15 JAN 2023 - Lilly Jade Katrin - lilly.kitty.1988@gmail.com
// Thanks to Stenzek and the PCSX2 project for inspiration, assistance and examples,
// to TheFetishMachine and Infernum for encouragement and cheerleading,
// and to Gollawiz, Sea, Fridge, jenette and Ryudo for testing

#pragma once

#include <QWidget>

#include <array>

class QCheckBox;
class QLineEdit;
class QGroupBox;
class QVBoxLayout;
class QPushButton;
class AchievementsWindow;

class AchievementSettingsWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit AchievementSettingsWidget(QWidget* parent, AchievementsWindow* parent_window);

private:
  void OnControllerInterfaceConfigure();

  void CreateLayout();
  void ConnectWidgets();

  void LoadSettings();
  void SaveSettings();

  void ToggleRAIntegration();
  void Login();
  void Logout();
  void ToggleAchievements();
  void ToggleLeaderboards();
  void ToggleRichPresence();
  void ToggleHardcore();
  void ToggleBadgeIcons();
  void ToggleUnofficial();
  void ToggleEncore();

  AchievementsWindow* parent_window;

  QGroupBox* m_common_box;
  QVBoxLayout* m_common_layout;
  QCheckBox* m_common_integration_enabled_input;
  QLabel* m_common_login_failed;
  QLabel* m_common_username_label;
  QLineEdit* m_common_username_input;
  QLabel* m_common_password_label;
  QLineEdit* m_common_password_input;
  QPushButton* m_common_login_button;
  QPushButton* m_common_logout_button;
  QCheckBox* m_common_achievements_enabled_input;
  QCheckBox* m_common_leaderboards_enabled_input;
  QCheckBox* m_common_rich_presence_enabled_input;
  QCheckBox* m_common_hardcore_enabled_input;
  QCheckBox* m_common_badge_icons_enabled_input;
  QCheckBox* m_common_unofficial_enabled_input;
  QCheckBox* m_common_encore_enabled_input;
};
