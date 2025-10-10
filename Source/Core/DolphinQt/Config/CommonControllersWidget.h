// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

#include <array>

class ConfigChoice;
class QGroupBox;
class QVBoxLayout;
class QPushButton;

class CommonControllersWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit CommonControllersWidget(QWidget* parent);

private:
  void OnControllerInterfaceConfigure();

  void CreateLayout();
  void ConnectWidgets();

  QGroupBox* m_common_box;
  QVBoxLayout* m_common_layout;
  ConfigChoice* m_common_accept_input_from;
  QPushButton* m_common_configure_controller_interface;
};
