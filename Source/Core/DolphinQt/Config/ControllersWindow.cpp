// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ControllersWindow.h"

#include <QDialogButtonBox>
#include <QVBoxLayout>

#include "DolphinQt/Config/CommonControllersWidget.h"
#include "DolphinQt/Config/GamecubeControllersWidget.h"
#include "DolphinQt/Config/WiimoteControllersWidget.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"

#if defined(CIFACE_USE_DUALSHOCKUDPCLIENT)
#include "DolphinQt/Config/ControllerInterface/DualShockUDPClientWidget.h"
#endif

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
  m_tab_widget = new QTabWidget();
  auto* layout = new QVBoxLayout();
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);

  m_tab_widget->addTab(m_gamecube_controllers, tr("GameCube"));
  m_tab_widget->addTab(m_wiimote_controllers, tr("Wii"));
  m_tab_widget->addTab(m_common, tr("Common"));
#if defined(CIFACE_USE_DUALSHOCKUDPCLIENT)
  m_dsuclient_widget = new DualShockUDPClientWidget();
  m_tab_widget->addTab(m_dsuclient_widget, tr("DSU Client"));  // TODO: use GetWrappedWidget()?
#endif
  layout->addWidget(m_tab_widget);
  layout->addStretch();
  layout->addWidget(m_button_box);

  WrapInScrollArea(this, layout);
}

void ControllersWindow::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
