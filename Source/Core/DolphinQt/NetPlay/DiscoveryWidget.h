// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QTableWidget>

#include "Core/NetPlayDiscovery.h"

class QTimer;

class DiscoveryWidget : public QTableWidget
{
  Q_OBJECT
public:
  explicit DiscoveryWidget(QWidget* parent = nullptr);
  ~DiscoveryWidget();

signals:
  void ServerSelected(const NetPlay::Discovery::DiscoveredServer& server);
  void ServerActivated(const NetPlay::Discovery::DiscoveredServer& server);

protected:
  void showEvent(QShowEvent* event) override;
  void hideEvent(QHideEvent* event) override;

private:
  void RefreshServers();

  void OnCellDoubleClicked(int row, int column);
  void OnSelectionChanged();

  NetPlay::Discovery::Client m_client{};

  QTimer* m_timer;
};
