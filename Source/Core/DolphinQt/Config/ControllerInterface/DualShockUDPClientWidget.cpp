// Copyright 2019 Dolphin Emulator Project5~5~5~
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/ControllerInterface/DualShockUDPClientWidget.h"

#include <fmt/format.h>

#include <QBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>

#include "Common/Config/Config.h"
#include "DolphinQt/Config/ControllerInterface/DualShockUDPClientAddServerDialog.h"
#include "DolphinQt/Config/ControllerInterface/DualShockUDPClientCalibrationDialog.h"
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

  // Note that you can always recalibrate
  m_calibrate = new QPushButton(tr("Calibrate Touch..."));
  m_add_server = new QPushButton(tr("Add..."));
  m_remove_server = new QPushButton(tr("Remove"));

  QHBoxLayout* hlayout = new QHBoxLayout;
  hlayout->addStretch();
  hlayout->addWidget(m_calibrate);
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

  OnServersEnabledToggled();
}

void DualShockUDPClientWidget::ConnectWidgets()
{
  connect(m_servers_enabled, &QCheckBox::clicked, this,
          &DualShockUDPClientWidget::OnServersEnabledToggled);
  connect(m_calibrate, &QPushButton::clicked, this, &DualShockUDPClientWidget::OnCalibration);
  connect(m_add_server, &QPushButton::clicked, this, &DualShockUDPClientWidget::OnServerAdded);
  connect(m_remove_server, &QPushButton::clicked, this, &DualShockUDPClientWidget::OnServerRemoved);
  connect(m_server_list, &QListWidget::itemSelectionChanged, this,
          &DualShockUDPClientWidget::OnServerSelectionChanged);
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
    // We have added even more settings now but they don't have to be defined
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

    // Index 3 and 4 are controller type and calibration data, there is no need to show them,
    // especially because the data might not be there and the default interpretation is up to
    // some other code, not us. Users can make a new one if they want to change these settings.
    QListWidgetItem* list_item = new QListWidgetItem(QString::fromStdString(
        fmt::format("{}:{} - {}", server_info[1], server_info[2], server_info[0])));
    m_server_list->addItem(list_item);
  }

  OnServerSelectionChanged();
}

void DualShockUDPClientWidget::OnServerAdded()
{
  DualShockUDPClientAddServerDialog add_server_dialog(this);
  connect(&add_server_dialog, &DualShockUDPClientAddServerDialog::accepted, this,
          &DualShockUDPClientWidget::RefreshServerList);
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

void DualShockUDPClientWidget::OnCalibration()
{
  // Enable calibration state and recreate the devices
  ciface::DualShockUDPClient::g_calibration_device_index = m_server_list->currentRow();
  // Instead of actually pointlessly editing a setting, we call OnConfigChanged()
  Config::OnConfigChanged();

  DualShockUDPClientCalibrationDialog calibration_dialog(this, m_server_list->currentRow());
  // These need to be called before RefreshServerList, it will destroy the calibration devices
  connect(&calibration_dialog, &DualShockUDPClientCalibrationDialog::accepted, this,
          &DualShockUDPClientWidget::OnCalibrationEnded);
  connect(&calibration_dialog, &DualShockUDPClientCalibrationDialog::rejected, this,
          &DualShockUDPClientWidget::OnCalibrationEnded);

  // Re-create all the UDP devices now, so that they will use the new calibration values
  connect(&calibration_dialog, &DualShockUDPClientCalibrationDialog::accepted, this,
          &DualShockUDPClientWidget::RefreshServerList);
  calibration_dialog.exec();
}

// Reset calibration state
void DualShockUDPClientWidget::OnCalibrationEnded()
{
  ciface::DualShockUDPClient::g_calibration_device_index = -1;
  ciface::DualShockUDPClient::g_calibration_device_found = false;
  ciface::DualShockUDPClient::g_calibration_touch_x_min = std::numeric_limits<s16>::max();
  ciface::DualShockUDPClient::g_calibration_touch_y_min = std::numeric_limits<s16>::max();
  ciface::DualShockUDPClient::g_calibration_touch_x_max = std::numeric_limits<s16>::min();
  ciface::DualShockUDPClient::g_calibration_touch_y_max = std::numeric_limits<s16>::min();
}

void DualShockUDPClientWidget::OnServersEnabledToggled()
{
  const bool checked = m_servers_enabled->isChecked();
  Config::SetBaseOrCurrent(ciface::DualShockUDPClient::Settings::SERVERS_ENABLED, checked);
  m_add_server->setEnabled(checked);

  const QSignalBlocker blocker(m_server_list);
  m_server_list->setEnabled(checked);
  m_server_list->setSelectionMode(checked ? QAbstractItemView::SelectionMode::SingleSelection :
                                            QAbstractItemView::SelectionMode::NoSelection);

  if (!checked)
  {
    m_server_list->clearSelection();
    m_server_list->setCurrentRow(-1);
  }

  OnServerSelectionChanged();
}

void DualShockUDPClientWidget::OnServerSelectionChanged()
{
  const bool any_selected = m_server_list->currentRow() >= 0;
  m_remove_server->setEnabled(any_selected);
  m_calibrate->setEnabled(any_selected);
}
