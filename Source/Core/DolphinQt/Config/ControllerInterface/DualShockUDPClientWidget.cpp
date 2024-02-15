// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ControllerInterface/DualShockUDPClientWidget.h"

#include <fmt/format.h>

#include <QBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>

#include "Common/Config/Config.h"
#include "DolphinQt/Config/ControllerInterface/DualShockUDPClientAddServerDialog.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "InputCommon/ControllerInterface/DualShockUDPClient/DualShockUDPClient.h"

DualShockUDPClientWidget::DualShockUDPClientWidget()
{
  CreateWidgets();
  ConnectWidgets();
}

void DualShockUDPClientWidget::CreateWidgets()
{
  auto* main_layout = new QVBoxLayout;

  m_servers_enabled = new QCheckBox(tr("Enable"));
  m_servers_enabled->setChecked(Config::Get(ciface::DualShockUDPClient::Settings::SERVERS_ENABLED));
  main_layout->addWidget(m_servers_enabled, 0, {});

  m_server_list = new QListWidget();
  main_layout->addWidget(m_server_list);

  m_add_server = new NonDefaultQPushButton(tr("Add..."));
  m_add_server->setEnabled(m_servers_enabled->isChecked());

  m_remove_server = new NonDefaultQPushButton(tr("Remove"));
  m_remove_server->setEnabled(m_servers_enabled->isChecked());

  QHBoxLayout* hlayout = new QHBoxLayout;
  hlayout->addStretch();
  hlayout->addWidget(m_add_server);
  hlayout->addWidget(m_remove_server);

  main_layout->addItem(hlayout);

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
  main_layout->addWidget(description);

  setLayout(main_layout);

  RefreshServerList();
}

void DualShockUDPClientWidget::ConnectWidgets()
{
  connect(m_servers_enabled, &QCheckBox::clicked, this,
          &DualShockUDPClientWidget::OnServersToggled);
  connect(m_add_server, &QPushButton::clicked, this, &DualShockUDPClientWidget::OnServerAdded);
  connect(m_remove_server, &QPushButton::clicked, this, &DualShockUDPClientWidget::OnServerRemoved);
}

void DualShockUDPClientWidget::RefreshServerList()
{
  m_server_list->clear();

  const auto server_address_setting =
      Config::Get(ciface::DualShockUDPClient::Settings::SERVER_ADDRESS);
  const auto server_port_setting = Config::Get(ciface::DualShockUDPClient::Settings::SERVER_PORT);

  // Update our servers setting if the user is using old configuration
  if (!server_address_setting.empty() && server_port_setting != 0)
  {
    const auto& servers_setting = Config::Get(ciface::DualShockUDPClient::Settings::SERVERS);
    Config::SetBaseOrCurrent(ciface::DualShockUDPClient::Settings::SERVERS,
                             servers_setting + fmt::format("{}:{}:{};", "DS4",
                                                           server_address_setting,
                                                           server_port_setting));
    Config::SetBase(ciface::DualShockUDPClient::Settings::SERVER_ADDRESS, "");
    Config::SetBase(ciface::DualShockUDPClient::Settings::SERVER_PORT, 0);
  }

  const auto& servers_setting = Config::Get(ciface::DualShockUDPClient::Settings::SERVERS);
  const auto server_details = SplitString(servers_setting, ';');
  for (const std::string& server_detail : server_details)
  {
    const auto server_info = SplitString(server_detail, ':');
    if (server_info.size() < 3)
      continue;

    QListWidgetItem* list_item = new QListWidgetItem(QString::fromStdString(
        fmt::format("{}:{} - {}", server_info[1], server_info[2], server_info[0])));
    m_server_list->addItem(list_item);
  }
  emit ConfigChanged();
}

void DualShockUDPClientWidget::OnServerAdded()
{
  DualShockUDPClientAddServerDialog add_server_dialog(this);
  connect(&add_server_dialog, &DualShockUDPClientAddServerDialog::accepted, this,
          &DualShockUDPClientWidget::RefreshServerList);
  SetQWidgetWindowDecorations(&add_server_dialog);
  add_server_dialog.exec();
}

void DualShockUDPClientWidget::OnServerRemoved()
{
  const int row_to_remove = m_server_list->currentRow();

  const auto& servers_setting = Config::Get(ciface::DualShockUDPClient::Settings::SERVERS);
  const auto server_details = SplitString(servers_setting, ';');

  std::string new_server_setting;
  for (int i = 0; i < m_server_list->count(); i++)
  {
    if (i == row_to_remove)
    {
      continue;
    }

    new_server_setting += server_details[i] + ';';
  }

  Config::SetBaseOrCurrent(ciface::DualShockUDPClient::Settings::SERVERS, new_server_setting);

  RefreshServerList();
}

void DualShockUDPClientWidget::OnServersToggled()
{
  bool checked = m_servers_enabled->isChecked();
  Config::SetBaseOrCurrent(ciface::DualShockUDPClient::Settings::SERVERS_ENABLED, checked);
  m_add_server->setEnabled(checked);
  m_remove_server->setEnabled(checked);
}
