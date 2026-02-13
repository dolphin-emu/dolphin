// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ControllerInterface/DualShockUDPClientEditServerDialog.h"

#include <fmt/format.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QWidget>

#include "DolphinQt/Config/ControllerInterface/ServerStringValidator.h"
#include "InputCommon/ControllerInterface/DualShockUDPClient/DualShockUDPClient.h"
#include "DolphinQt/Config/ControllerInterface/DualShockUDPSettings.h"

DualShockUDPClientEditServerDialog::DualShockUDPClientEditServerDialog(QWidget* parent, std::optional<size_t> existing_index)
    : QDialog(parent), m_existing_index(existing_index)
{
  CreateWidgets();
  setLayout(m_main_layout);
}

void DualShockUDPClientEditServerDialog::CreateWidgets()
{
  setWindowTitle(tr(m_existing_index.has_value() ? "Edit DSU Server" : "Add New DSU Server"));

  m_main_layout = new QGridLayout;

  m_description = new QLineEdit();
  m_description->setPlaceholderText(tr("BetterJoy, DS4Windows, etc"));
  m_description->setValidator(new ServerStringValidator(m_description));

  m_server_address =
      new QLineEdit(QString::fromStdString(ciface::DualShockUDPClient::DEFAULT_SERVER_ADDRESS));
  m_server_address->setValidator(new ServerStringValidator(m_server_address));

  m_server_port = new QSpinBox();
  m_server_port->setMaximum(65535);
  m_server_port->setValue(ciface::DualShockUDPClient::DEFAULT_SERVER_PORT);

  if (m_existing_index.has_value())
  {
    const auto server = DualShockUDPSettings::GetServers()[m_existing_index.value()];
    m_description->setText(QString::fromStdString(server.description));
    m_server_address->setText(QString::fromStdString(server.server_address));
    m_server_port->setValue(server.server_port);
  }

  m_main_layout->addWidget(new QLabel(tr("Description")), 1, 0);
  m_main_layout->addWidget(m_description, 1, 1);
  m_main_layout->addWidget(new QLabel(tr("Server IP Address")), 2, 0);
  m_main_layout->addWidget(m_server_address, 2, 1);
  m_main_layout->addWidget(new QLabel(tr("Server Port")), 3, 0);
  m_main_layout->addWidget(m_server_port, 3, 1);

  m_buttonbox = new QDialogButtonBox();
  auto* finish_button = new QPushButton(tr(m_existing_index ? "Apply" : "Add"));
  auto* cancel_button = new QPushButton(tr("Cancel"));
  m_buttonbox->addButton(finish_button, QDialogButtonBox::AcceptRole);
  m_buttonbox->addButton(cancel_button, QDialogButtonBox::RejectRole);
  connect(finish_button, &QPushButton::clicked, this,
          &DualShockUDPClientEditServerDialog::OnServerFinished);
  connect(cancel_button, &QPushButton::clicked, this, &DualShockUDPClientEditServerDialog::reject);
  finish_button->setDefault(true);

  m_main_layout->addWidget(m_buttonbox, 4, 0, 1, 2);
}

void DualShockUDPClientEditServerDialog::OnServerFinished()
{
  const auto server = DualShockUDPServer(m_description->text().toStdString(),
                                          m_server_address->text().toStdString(),
                                          m_server_port->value());
  if (m_existing_index.has_value())
  {
    DualShockUDPSettings::ReplaceServer(m_existing_index.value(), server);
  }
  else
  {
    DualShockUDPSettings::AddServer(server);
  }
  accept();
}
