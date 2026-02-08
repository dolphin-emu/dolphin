// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ControllerInterface/ControllerInterfaceWindow.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

#if defined(CIFACE_USE_DUALSHOCKUDPCLIENT)
#include "DolphinQt/Config/ControllerInterface/DualShockUDPClientWidget.h"
#endif

ControllerInterfaceWindow::ControllerInterfaceWindow(QWidget* parent) : QDialog(parent)
{
  CreateMainLayout();

  setWindowTitle(tr("Alternate Input Sources"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

void ControllerInterfaceWindow::CreateMainLayout()
{
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  m_tab_widget = new QTabWidget();
#if defined(CIFACE_USE_DUALSHOCKUDPCLIENT)
  m_dsuclient_widget = new DualShockUDPClientWidget();
  m_tab_widget->addTab(m_dsuclient_widget, tr("DSU Client"));  // TODO: use GetWrappedWidget()?
#endif

  auto* main_layout = new QVBoxLayout();
  if (m_tab_widget->count() > 0)
  {
    main_layout->addWidget(m_tab_widget);
  }
  else
  {
    main_layout->addWidget(new QLabel(tr("Nothing to configure")), 0,
                           Qt::AlignVCenter | Qt::AlignHCenter);
  }
  main_layout->addWidget(m_button_box);
  setLayout(main_layout);
}
