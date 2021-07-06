// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

#include <array>

class QComboBox;
class QHBoxLayout;
class QGridLayout;
class QGroupBox;
class QPushButton;

class GamecubeControllersWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit GamecubeControllersWidget(QWidget* parent);

private:
  void LoadSettings();
  void SaveSettings();

  void OnGCTypeChanged(int state);
  void OnGCPadConfigure();

  void CreateLayout();
  void ConnectWidgets();

  // Gamecube
  QGroupBox* m_gc_box;
  QGridLayout* m_gc_layout;
  std::array<QComboBox*, 4> m_gc_controller_boxes;
  std::array<QPushButton*, 4> m_gc_buttons;
  std::array<QHBoxLayout*, 4> m_gc_groups;
};
