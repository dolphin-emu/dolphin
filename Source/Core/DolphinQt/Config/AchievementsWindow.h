// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// 15 JAN 2023 - Lilly Jade Katrin - lilly.kitty.1988@gmail.com
// Thanks to Stenzek and the PCSX2 project for inspiration, assistance and examples,
// to TheFetishMachine and Infernum for encouragement and cheerleading,
// and to Gollawiz, Sea, Fridge, jenette and Ryudo for testing

#pragma once
#include <QDialog>

class QTabWidget;
class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QProgressBar;
class AchievementProgressWidget;
class AchievementLeaderboardWidget;

class AchievementsWindow : public QDialog
{
  Q_OBJECT
public:
  explicit AchievementsWindow(QWidget* parent);
  void UpdateData();

private:
  void CreateMainLayout();
  void showEvent(QShowEvent* event);
  void CreateGeneralBlock();
  void UpdateGeneralBlock();
  void ConnectWidgets();

  AchievementProgressWidget* progress_widget;
  AchievementLeaderboardWidget* leaderboard_widget;

  QGroupBox* m_general_box;
  QDialogButtonBox* m_button_box;
  QTabWidget* m_tab_widget;

  QLabel* m_user_icon;
  QLabel* m_user_name;
  QLabel* m_user_points;
  QLabel* m_user_icon_2;
  QLabel* m_game_icon;
  QLabel* m_game_name;
  QLabel* m_game_points;
  QProgressBar* m_game_progress_hard;
  QProgressBar* m_game_progress_soft;
  QLabel* m_rich_presence;

  QGroupBox* m_user_box;
  QGroupBox* m_game_box;

  // TODO lillyjade: IHATEITIHATEITIHATEITIHATEIT
  // Find a way to call updates on the cheevo window from cheevo manager
  // via events instead of refreshing every period please
  QTimer* m_update_timer;
};
