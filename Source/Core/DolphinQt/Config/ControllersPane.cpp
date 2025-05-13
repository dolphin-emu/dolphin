// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ControllersPane.h"

#include <QVBoxLayout>

#include "DolphinQt/Config/CommonControllersWidget.h"
#include "DolphinQt/Config/GamecubeControllersWidget.h"
#include "DolphinQt/Config/WiimoteControllersWidget.h"

ControllersPane::ControllersPane()
{
  CreateMainLayout();
}

void ControllersPane::showEvent(QShowEvent* event)
{
  QWidget::showEvent(event);

  m_wiimote_controllers->UpdateBluetoothAvailableStatus();
  m_wiimote_controllers->RefreshBluetoothAdapters();
}

void ControllersPane::CreateMainLayout()
{
  auto* const layout = new QVBoxLayout{this};

  auto* const gamecube_controllers = new GamecubeControllersWidget(this);
  m_wiimote_controllers = new WiimoteControllersWidget(this);
  auto* const common = new CommonControllersWidget(this);

  layout->addWidget(gamecube_controllers);
  layout->addWidget(m_wiimote_controllers);
  layout->addWidget(common);
}
