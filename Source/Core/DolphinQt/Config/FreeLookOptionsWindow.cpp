// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/FreeLookOptionsWindow.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

#include "Core/Config/FreeLookSettings.h"
#include "DolphinQt/Config/FreeLookOptionsWidget.h"

FreeLookOptionsWindow::FreeLookOptionsWindow(QWidget* parent, int index)
    : QDialog(parent), m_port(index)
{
  CreateMainLayout();

  setWindowTitle(tr("Free Look Camera %1 Options").arg(index + 1));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

void FreeLookOptionsWindow::CreateMainLayout()
{
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto* main_layout = new QVBoxLayout();
  if (m_port == 0)
  {
    main_layout->addWidget(
        new FreeLookOptionsWidget(this, ::Config::FL1_FOV_HORIZONTAL, ::Config::FL1_FOV_VERTICAL));
  }
  main_layout->addWidget(m_button_box);
  setLayout(main_layout);
}
