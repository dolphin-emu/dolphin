// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/DiscoveryWidget.h"

#include <QHeaderView>
#include <QTableWidget>
#include <QTimer>

static QString GetPlatformName(NetPlay::Discovery::Platform platform)
{
  switch (platform)
  {
  case NetPlay::Discovery::Platform::Linux:
    return QStringLiteral("Linux");
  case NetPlay::Discovery::Platform::Windows:
    return QStringLiteral("Windows");
  case NetPlay::Discovery::Platform::macOS:
    return QStringLiteral("macOS");
  case NetPlay::Discovery::Platform::Android:
    return QStringLiteral("Android");
  case NetPlay::Discovery::Platform::iOS:
    return QStringLiteral("iOS");
  case NetPlay::Discovery::Platform::FreeBSD:
    return QStringLiteral("FreeBSD");
  case NetPlay::Discovery::Platform::OpenBSD:
    return QStringLiteral("OpenBSD");
  case NetPlay::Discovery::Platform::NetBSD:
    return QStringLiteral("NetBSD");
  case NetPlay::Discovery::Platform::DragonFlyBSD:
    return QStringLiteral("DragonFly BSD");
  case NetPlay::Discovery::Platform::Haiku:
    return QStringLiteral("Haiku");
  default:
    return QStringLiteral("Unknown");
  }
}

DiscoveryWidget::DiscoveryWidget(QWidget* parent) : QTableWidget(0, 5, parent)
{
  setHorizontalHeaderLabels({tr("Platform"), tr("Host"), tr("Game"), tr("Players"), tr("Version")});
  verticalHeader()->setVisible(false);
  setSelectionBehavior(QAbstractItemView::SelectRows);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
  horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
  horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

  connect(this, &QTableWidget::cellDoubleClicked, this, &DiscoveryWidget::OnCellDoubleClicked);
  connect(this, &QTableWidget::itemSelectionChanged, this, &DiscoveryWidget::OnSelectionChanged);

  m_timer = new QTimer(this);
  connect(m_timer, &QTimer::timeout, this, &DiscoveryWidget::RefreshServers);
}

DiscoveryWidget::~DiscoveryWidget()
{
  if (m_timer->isActive())
    m_timer->stop();
  m_client.Stop();
}

void DiscoveryWidget::showEvent(QShowEvent* event)
{
  QTableWidget::showEvent(event);

  if (!m_timer->isActive())
  {
    m_client.Start();
    RefreshServers();
    m_timer->start(1000);
  }
}

void DiscoveryWidget::hideEvent(QHideEvent* event)
{
  QTableWidget::hideEvent(event);

  if (m_timer->isActive())
  {
    m_timer->stop();
    m_client.Stop();
    clearContents();
  }
}

void DiscoveryWidget::RefreshServers()
{
  using NetPlay::Discovery::DiscoveredServer;
  const std::vector<DiscoveredServer> servers = m_client.GetServers();

  {
    const QSignalBlocker blocker(this);
    setRowCount(static_cast<int>(servers.size()));

    for (int row = 0; row < static_cast<int>(servers.size()); ++row)
    {
      const DiscoveredServer& server = servers[row];

      auto* platform_item = new QTableWidgetItem(GetPlatformName(server.payload.platform));
      platform_item->setData(Qt::UserRole, QVariant::fromValue(server));

      auto* host_item = new QTableWidgetItem(QString::fromStdString(server.payload.server_name));
      auto* game_item = new QTableWidgetItem(server.payload.game_name.empty() ?
                                                 tr("No game selected") :
                                                 QString::fromStdString(server.payload.game_name));
      auto* players_item = new QTableWidgetItem(QString::number(server.payload.player_count));
      players_item->setTextAlignment(Qt::AlignCenter);
      auto* version_item = new QTableWidgetItem(QString::fromStdString(server.payload.version));

      setItem(row, 0, platform_item);
      setItem(row, 1, host_item);
      setItem(row, 2, game_item);
      setItem(row, 3, players_item);
      setItem(row, 4, version_item);
    }
  }
}

void DiscoveryWidget::OnSelectionChanged()
{
  const auto selected = selectionModel()->selectedRows();
  if (selected.isEmpty())
    return;

  auto* platform_item = item(selected.first().row(), 0);
  if (!platform_item)
    return;

  emit ServerSelected(
      platform_item->data(Qt::UserRole).value<NetPlay::Discovery::DiscoveredServer>());
}

void DiscoveryWidget::OnCellDoubleClicked(int row, int column)
{
  auto* platform_item = item(row, 0);
  if (!platform_item)
    return;

  emit ServerActivated(
      platform_item->data(Qt::UserRole).value<NetPlay::Discovery::DiscoveredServer>());
}
