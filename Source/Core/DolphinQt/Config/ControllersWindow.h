// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

class CommonControllersWidget;
class GamecubeControllersWidget;
class QDialogButtonBox;
class WiimoteControllersWidget;

class ControllersWindow final : public QDialog
{
  Q_OBJECT
public:
  explicit ControllersWindow(QWidget* parent);

private:
  void CreateMainLayout();
  void ConnectWidgets();

  QDialogButtonBox* m_button_box;
  GamecubeControllersWidget* m_gamecube_controllers;
  WiimoteControllersWidget* m_wiimote_controllers;
  CommonControllersWidget* m_common;
};
