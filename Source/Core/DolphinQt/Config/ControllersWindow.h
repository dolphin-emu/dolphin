// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class CommonControllersWidget;
class GamecubeControllersWidget;
class QDialogButtonBox;
class QShowEvent;
class WiimoteControllersWidget;

class ControllersWindow final : public QDialog
{
  Q_OBJECT
public:
  explicit ControllersWindow(QWidget* parent);

protected:
  void showEvent(QShowEvent* event) override;

private:
  void CreateMainLayout();
  void ConnectWidgets();

  QDialogButtonBox* m_button_box;
  GamecubeControllersWidget* m_gamecube_controllers;
  WiimoteControllersWidget* m_wiimote_controllers;
  CommonControllersWidget* m_common;
};
