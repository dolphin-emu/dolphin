// Copyright 2019 Dolphin Emulator Project5~5~5~
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/ControllerInterface/DualShockUDPClientWidget.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

#include "Common/Config/Config.h"
#include "InputCommon/ControllerInterface/DualShockUDPClient/DualShockUDPClient.h"

DualShockUDPClientWidget::DualShockUDPClientWidget()
{
  CreateWidgets();
  ConnectWidgets();
}

void DualShockUDPClientWidget::CreateWidgets()
{
  auto* main_layout = new QGridLayout;

  m_server_enabled = new QCheckBox(tr("Enable"));
  m_server_enabled->setChecked(Config::Get(ciface::DualShockUDPClient::Settings::SERVER_ENABLED));

  m_server_address = new QLineEdit(
      QString::fromStdString(Config::Get(ciface::DualShockUDPClient::Settings::SERVER_ADDRESS)));
  m_server_address->setEnabled(m_server_enabled->isChecked());

  m_server_port = new QSpinBox();
  m_server_port->setMaximum(65535);
  m_server_port->setValue(Config::Get(ciface::DualShockUDPClient::Settings::SERVER_PORT));
  m_server_port->setEnabled(m_server_enabled->isChecked());

  auto* description =
      new QLabel(tr("DSU protocol enables the use of input and motion data from compatible "
                    "sources, like PlayStation, Nintendo Switch and Steam controllers.<br><br>"
                    "For setup instructions, "
                    "<a href=\"https://wiki.dolphin-emu.org/index.php?title=DSU_Client\">"
                    "refer to this page</a>."));
  description->setTextFormat(Qt::RichText);
  description->setWordWrap(true);
  description->setTextInteractionFlags(Qt::TextBrowserInteraction);
  description->setOpenExternalLinks(true);

  main_layout->addWidget(m_server_enabled, 1, 1);
  main_layout->addWidget(new QLabel(tr("Server IP Address")), 2, 1);
  main_layout->addWidget(m_server_address, 2, 2);
  main_layout->addWidget(new QLabel(tr("Server Port")), 3, 1);
  main_layout->addWidget(m_server_port, 3, 2);
  main_layout->addWidget(description, 4, 1, 1, 2);

  setLayout(main_layout);
}

void DualShockUDPClientWidget::ConnectWidgets()
{
  connect(m_server_enabled, &QCheckBox::toggled, this, [this] {
    bool checked = m_server_enabled->isChecked();
    Config::SetBaseOrCurrent(ciface::DualShockUDPClient::Settings::SERVER_ENABLED, checked);
    m_server_address->setEnabled(checked);
    m_server_port->setEnabled(checked);
  });

  connect(m_server_address, &QLineEdit::editingFinished, this, [this] {
    Config::SetBaseOrCurrent(ciface::DualShockUDPClient::Settings::SERVER_ADDRESS,
                             m_server_address->text().toStdString());
  });

  connect(m_server_port, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
          [this] {
            Config::SetBaseOrCurrent(ciface::DualShockUDPClient::Settings::SERVER_PORT,
                                     static_cast<u16>(m_server_port->value()));
          });
}
