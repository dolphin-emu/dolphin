// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ControllersWindow.h"

#include <QDialogButtonBox>
#include <QVBoxLayout>

#include "DolphinQt/Config/CommonControllersWidget.h"
#include "DolphinQt/Config/GamecubeControllersWidget.h"
#include "DolphinQt/Config/WiimoteControllersWidget.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"

ControllersWindow::ControllersWindow(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Controller Settings"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  m_gamecube_controllers = new GamecubeControllersWidget(this);
  m_wiimote_controllers = new WiimoteControllersWidget(this);
  m_common = new CommonControllersWidget(this);
  CreateMainLayout();
  ConnectWidgets();
}

void ControllersWindow::showEvent(QShowEvent* event)
{
  QDialog::showEvent(event);
  m_wiimote_controllers->UpdateBluetoothAvailableStatus();
}

void ControllersWindow::CreateMainLayout()
{
  auto* const grid = new QGridLayout;
  grid->addWidget(m_gamecube_controllers, 0, 0);
  grid->addWidget(m_wiimote_controllers, 0, 1, 2, 1);
  grid->addWidget(m_common, 1, 0);

  auto* const layout = new QVBoxLayout;
  layout->addLayout(grid);

  layout->addStretch();
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);
  layout->addWidget(m_button_box);

  setLayout(layout);
}

void ControllersWindow::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
