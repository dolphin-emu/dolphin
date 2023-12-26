// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS
#include <QWidget>

class QLabel;
class QPushButton;

class HardcoreWarningWidget : public QWidget
{
  Q_OBJECT
public:
  explicit HardcoreWarningWidget(QWidget* parent);

signals:
  void OpenAchievementSettings();

private:
  void CreateWidgets();
  void ConnectWidgets();

  void Update();

  QLabel* m_text;
  QPushButton* m_settings_button;
};
#endif  // USE_RETRO_ACHIEVEMENTS
