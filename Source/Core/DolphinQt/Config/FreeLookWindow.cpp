// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/FreeLookWindow.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

#include "DolphinQt/Config/FreeLook2DWidget.h"
#include "DolphinQt/Config/FreeLookWidget.h"

FreeLookWindow::FreeLookWindow(QWidget* parent) : QDialog(parent)
{
  CreateMainLayout();

  setWindowTitle(tr("Free Look Settings"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

void FreeLookWindow::CreateMainLayout()
{
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto* tab_widget = new QTabWidget();
  tab_widget->addTab(new FreeLookWidget(this), tr("3D"));
  tab_widget->addTab(new FreeLook2DWidget(this), tr("2D"));

  auto* main_layout = new QVBoxLayout();
  main_layout->addWidget(tab_widget);
  main_layout->addWidget(m_button_box);
  setLayout(main_layout);
}
