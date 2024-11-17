// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

#if defined(CIFACE_USE_DUALSHOCKUDPCLIENT)
class DualShockUDPClientWidget;
#endif
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
  QTabWidget* m_tab_widget;
  GamecubeControllersWidget* m_gamecube_controllers;
  WiimoteControllersWidget* m_wiimote_controllers;
  CommonControllersWidget* m_common;

#if defined(CIFACE_USE_DUALSHOCKUDPCLIENT)
  DualShockUDPClientWidget* m_dsuclient_widget;
#endif
};
