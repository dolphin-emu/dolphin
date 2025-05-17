// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QGroupBox>

class QCheckBox;
class QPushButton;

class CommonControllersWidget final : public QGroupBox
{
  Q_OBJECT
public:
  explicit CommonControllersWidget(QWidget* parent);

private:
  void OnControllerInterfaceConfigure();

  void CreateLayout();
  void ConnectWidgets();

  void LoadSettings();
  void SaveSettings();

  QCheckBox* m_common_bg_input;
  QPushButton* m_common_configure_controller_interface;
};
