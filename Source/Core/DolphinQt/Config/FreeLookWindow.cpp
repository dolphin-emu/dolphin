// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/FreeLookWindow.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

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

  auto* main_layout = new QVBoxLayout();
  main_layout->addWidget(new FreeLookWidget(this));
  main_layout->addWidget(m_button_box);
  setLayout(main_layout);
}
