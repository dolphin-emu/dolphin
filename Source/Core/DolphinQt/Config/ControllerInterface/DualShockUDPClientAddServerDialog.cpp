// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ControllerInterface/DualShockUDPClientAddServerDialog.h"

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

#include "Common/Config/Config.h"
#include "DolphinQt/Config/ControllerInterface/ServerStringValidator.h"
#include "InputCommon/ControllerInterface/DualShockUDPClient/DualShockUDPClient.h"

DualShockUDPClientAddServerDialog::DualShockUDPClientAddServerDialog(QWidget* parent)
    : QDialog(parent)
{
  CreateWidgets();
  setLayout(m_main_layout);
}

void DualShockUDPClientAddServerDialog::CreateWidgets()
{
  setWindowTitle(tr("Add New DSU Server"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

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

  m_main_layout->addWidget(new QLabel(tr("Description")), 1, 0);
  m_main_layout->addWidget(m_description, 1, 1);
  m_main_layout->addWidget(new QLabel(tr("Server IP Address")), 2, 0);
  m_main_layout->addWidget(m_server_address, 2, 1);
  m_main_layout->addWidget(new QLabel(tr("Server Port")), 3, 0);
  m_main_layout->addWidget(m_server_port, 3, 1);

  m_buttonbox = new QDialogButtonBox();
  auto* add_button = new QPushButton(tr("Add"));
  auto* cancel_button = new QPushButton(tr("Cancel"));
  m_buttonbox->addButton(add_button, QDialogButtonBox::AcceptRole);
  m_buttonbox->addButton(cancel_button, QDialogButtonBox::RejectRole);
  connect(add_button, &QPushButton::clicked, this,
          &DualShockUDPClientAddServerDialog::OnServerAdded);
  connect(cancel_button, &QPushButton::clicked, this, &DualShockUDPClientAddServerDialog::reject);
  add_button->setDefault(true);

  m_main_layout->addWidget(m_buttonbox, 4, 0, 1, 2);
}

void DualShockUDPClientAddServerDialog::OnServerAdded()
{
  const auto& servers_setting = Config::Get(ciface::DualShockUDPClient::Settings::SERVERS);
  Config::SetBaseOrCurrent(ciface::DualShockUDPClient::Settings::SERVERS,
                           servers_setting + fmt::format("{}:{}:{};",
                                                         m_description->text().toStdString(),
                                                         m_server_address->text().toStdString(),
                                                         m_server_port->value()));
  accept();
}
