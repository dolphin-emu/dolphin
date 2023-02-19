// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// 25 JAN 2023 - Lilly Jade Katrin - lilly.kitty.1988@gmail.com
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

struct rc_api_achievement_definition_t;

class AchievementProgressWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit AchievementProgressWidget(QWidget* parent);
  void UpdateData();

private:
  void OnControllerInterfaceConfigure();

  QGroupBox* CreateAchievementBox(const rc_api_achievement_definition_t* achievement);

  QGroupBox* m_common_box;
  QVBoxLayout* m_common_layout;
};
