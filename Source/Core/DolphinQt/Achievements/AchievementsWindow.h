// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS
#include <QDialog>

class QTabWidget;
class QDialogButtonBox;

class AchievementsWindow : public QDialog
{
  Q_OBJECT
public:
  explicit AchievementsWindow(QWidget* parent);
  void UpdateData();

private:
  void CreateMainLayout();
  void showEvent(QShowEvent* event);
  void ConnectWidgets();

  QTabWidget* m_tab_widget;
  QDialogButtonBox* m_button_box;
};

#endif  // USE_RETRO_ACHIEVEMENTS
