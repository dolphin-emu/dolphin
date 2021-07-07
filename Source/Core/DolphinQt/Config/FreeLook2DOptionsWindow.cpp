// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/FreeLook2DOptionsWindow.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

#include "Core/Config/FreeLookSettings.h"
#include "DolphinQt/Config/FreeLook2DOptionsWidget.h"

FreeLook2DOptionsWindow::FreeLook2DOptionsWindow(QWidget* parent, int index)
    : QDialog(parent), m_port(index)
{
  CreateMainLayout();

  setWindowTitle(tr("Free Look 2D Camera %1 Options").arg(index + 1));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

void FreeLook2DOptionsWindow::CreateMainLayout()
{
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto* main_layout = new QVBoxLayout();
  if (m_port == 0)
  {
    main_layout->addWidget(new FreeLook2DOptionsWidget(this));
  }
  main_layout->addWidget(m_button_box);
  setLayout(main_layout);
}
