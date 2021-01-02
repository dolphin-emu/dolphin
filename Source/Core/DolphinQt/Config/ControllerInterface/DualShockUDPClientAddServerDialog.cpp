// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/ControllerInterface/DualShockUDPClientAddServerDialog.h"

#include <fmt/format.h>

#include <QComboBox>
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
  setWindowTitle(tr("Add DSU Server"));
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

  m_type = new QComboBox();
  m_type->setToolTip(tr(
      "This will be exclusived used for input names and default calibration.\nNames aren't "
      "guaranteed to be right for "
      "non DS4/DualSense controllers\nor if the DSU source uses remapping.\nDifferent DSU profile "
      "types also won't be compatible with each other.\nThis will apply to all devices connected "
      "from this server, but you can\nstill create the same server twice and use "
      "different devices for each."));
  m_type->addItem(tr("DualShock 4"), QStringLiteral("DS4"));
  m_type->addItem(tr("DualSense"), QStringLiteral("DualSense"));
  m_type->addItem(tr("Switch"), QStringLiteral("Switch"));
  m_type->addItem(tr("Generic"), QStringLiteral("Generic"));

  m_main_layout->addWidget(new QLabel(tr("Description")), 1, 0);
  m_main_layout->addWidget(m_description, 1, 1);
  m_main_layout->addWidget(new QLabel(tr("Server IP Address")), 2, 0);
  m_main_layout->addWidget(m_server_address, 2, 1);
  m_main_layout->addWidget(new QLabel(tr("Server Port")), 3, 0);
  m_main_layout->addWidget(m_server_port, 3, 1);
  m_main_layout->addWidget(new QLabel(tr("Type")), 4, 0);
  m_main_layout->addWidget(m_type, 4, 1);

  m_buttonbox = new QDialogButtonBox();
  auto* add_button = new QPushButton(tr("Add"));
  auto* cancel_button = new QPushButton(tr("Cancel"));
  m_buttonbox->addButton(add_button, QDialogButtonBox::AcceptRole);
  m_buttonbox->addButton(cancel_button, QDialogButtonBox::RejectRole);
  connect(add_button, &QPushButton::clicked, this,
          &DualShockUDPClientAddServerDialog::OnServerAdded);
  connect(cancel_button, &QPushButton::clicked, this, &DualShockUDPClientAddServerDialog::reject);
  add_button->setDefault(true);

  m_main_layout->addWidget(m_buttonbox, 5, 0, 1, 2);
}

void DualShockUDPClientAddServerDialog::OnServerAdded()
{
  const auto& servers_setting = Config::Get(ciface::DualShockUDPClient::Settings::SERVERS);
  // We can't calibrate them now because the input device don't exist yet, it's not connected,
  // so just pass in empty calibration information, users will be able to calibrate themselves,
  // or default calibration will be used.
  const std::string device_type = m_type->currentData().toString().toStdString();
  if (device_type.empty())
  {
    Config::SetBaseOrCurrent(ciface::DualShockUDPClient::Settings::SERVERS,
                             servers_setting + fmt::format("{}:{}:{};",
                                                           m_description->text().toStdString(),
                                                           m_server_address->text().toStdString(),
                                                           m_server_port->value()));
  }
  else
  {
    Config::SetBaseOrCurrent(ciface::DualShockUDPClient::Settings::SERVERS,
                             servers_setting + fmt::format("{}:{}:{}:{};",
                                                           m_description->text().toStdString(),
                                                           m_server_address->text().toStdString(),
                                                           m_server_port->value(), device_type));
  }
  accept();
}
