// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ControllerInterface/DualShockUDPClientWidget.h"

#include <fmt/format.h>

#include <QBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>

#include "DolphinQt/Config/ControllerInterface/DualShockUDPClientEditServerDialog.h"
#include "DolphinQt/Config/ControllerInterface/DualShockUDPSettings.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"

DualShockUDPClientWidget::DualShockUDPClientWidget()
{
  CreateWidgets();
  ConnectWidgets();
}

void DualShockUDPClientWidget::CreateWidgets()
{
  auto* main_layout = new QVBoxLayout;

  m_servers_enabled = new QCheckBox(tr("Enable"));
  m_servers_enabled->setChecked(DualShockUDPSettings::IsEnabled());
  main_layout->addWidget(m_servers_enabled, 0, {});

  m_server_list = new QListWidget();
  main_layout->addWidget(m_server_list);

  m_add_server = new NonDefaultQPushButton(tr("Add..."));
  m_edit_server = new NonDefaultQPushButton(tr("Edit"));
  m_remove_server = new NonDefaultQPushButton(tr("Remove"));

  QHBoxLayout* hlayout = new QHBoxLayout;
  hlayout->addStretch();
  hlayout->addWidget(m_add_server);
  hlayout->addWidget(m_edit_server);
  hlayout->addWidget(m_remove_server);

  main_layout->addItem(hlayout);

  auto* description =
      new QLabel(tr("The DSU protocol enables the use of input and motion data from compatible "
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
  connect(m_add_server, &QPushButton::clicked, this, &DualShockUDPClientWidget::OnServerAdded);
  connect(m_edit_server, &QPushButton::clicked, this, &DualShockUDPClientWidget::OnServerEdited);
  connect(m_remove_server, &QPushButton::clicked, this, &DualShockUDPClientWidget::OnServerRemoved);
  connect(m_server_list, &QListWidget::currentRowChanged, this,
          &DualShockUDPClientWidget::OnServerSelection);
  connect(m_servers_enabled, &QCheckBox::clicked, this,
          &DualShockUDPClientWidget::OnServersToggled);
}

void DualShockUDPClientWidget::SetButtonEnableStates()
{
  const bool has_selection = m_server_list->currentRow() != -1;
  m_edit_server->setEnabled(has_selection);
  m_remove_server->setEnabled(has_selection);
}

void DualShockUDPClientWidget::RefreshServerList()
{
  m_server_list->clear();

  for (const auto& server : DualShockUDPSettings::GetServers())
  {
    QListWidgetItem* list_item = new QListWidgetItem(QString::fromStdString(
        fmt::format("{}:{} - {}", server.description, server.server_address, server.server_port)));
    m_server_list->addItem(list_item);
  }

  SetButtonEnableStates();

  emit ConfigChanged();
}

void DualShockUDPClientWidget::OnServerAdded()
{
  DualShockUDPClientEditServerDialog add_server_dialog(this, std::nullopt);
  connect(&add_server_dialog, &DualShockUDPClientEditServerDialog::accepted, this,
          &DualShockUDPClientWidget::RefreshServerList);
  add_server_dialog.exec();
}

void DualShockUDPClientWidget::OnServerEdited()
{
  DualShockUDPClientEditServerDialog edit_server_dialog(this, m_server_list->currentRow());
  connect(&edit_server_dialog, &DualShockUDPClientEditServerDialog::accepted, this,
          &DualShockUDPClientWidget::RefreshServerList);
  edit_server_dialog.exec();
}

void DualShockUDPClientWidget::OnServerRemoved()
{
  const int row_to_remove = m_server_list->currentRow();

  DualShockUDPSettings::RemoveServer(row_to_remove);

  RefreshServerList();
}

void DualShockUDPClientWidget::OnServerSelection()
{
  SetButtonEnableStates();
}

void DualShockUDPClientWidget::OnServersToggled()
{
  DualShockUDPSettings::SetEnabled(m_servers_enabled->isChecked());
}
