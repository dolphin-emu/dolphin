// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/FreeLookUDPOptionsWidget.h"

#include <QFormLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QSpinBox>

#include "Common/Config/Config.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "DolphinQt/Settings.h"

FreeLookUDPOptionsWidget::FreeLookUDPOptionsWidget(
    QWidget* parent, const Config::Info<std::string>& udp_address_config,
    const Config::Info<u16>& udp_port_config)
    : QWidget(parent), m_udp_address_config(udp_address_config), m_udp_port_config(udp_port_config)
{
  CreateLayout();
  LoadSettings();
  ConnectWidgets();
}

void FreeLookUDPOptionsWidget::CreateLayout()
{
  auto* layout = new QFormLayout;
  m_server_address = new QLineEdit(this);
  m_server_port = new QSpinBox(this);
  m_server_port->setMaximum(65535);
  layout->addRow(tr("Address:"), m_server_address);
  layout->addRow(tr("Port:"), m_server_port);
  setLayout(layout);
}

void FreeLookUDPOptionsWidget::ConnectWidgets()
{
  connect(m_server_address, &QLineEdit::textChanged, this, &FreeLookUDPOptionsWidget::SaveSettings);
  connect(m_server_port, qOverload<int>(&QSpinBox::valueChanged), this,
          &FreeLookUDPOptionsWidget::SaveSettings);
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this] {
    const QSignalBlocker blocker(this);
    LoadSettings();
  });
}

void FreeLookUDPOptionsWidget::LoadSettings()
{
  m_server_address->setText(QString::fromStdString(::Config::Get(m_udp_address_config)));
  m_server_port->setValue(::Config::Get(m_udp_port_config));
}

void FreeLookUDPOptionsWidget::SaveSettings()
{
  ::Config::SetBaseOrCurrent(m_udp_address_config, m_server_address->text().toStdString());
  ::Config::SetBaseOrCurrent(m_udp_port_config, static_cast<u16>(m_server_port->value()));
}
