// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/FreeLookOptionsWindow.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

#include "Core/Config/FreeLookSettings.h"
#include "Core/FreeLookConfig.h"
#include "DolphinQt/Config/FreeLookUDPOptionsWidget.h"

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
    if (Config::Get(Config::FL1_CONTROL_TYPE) == FreeLook::ControlType::UDP)
    {
      main_layout->addWidget(
          new FreeLookUDPOptionsWidget(this, ::Config::FL1_UDP_ADDRESS, ::Config::FL1_UDP_PORT));
    }
  }
  main_layout->addWidget(m_button_box);
  setLayout(main_layout);
}
