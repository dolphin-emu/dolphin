// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class QCheckBox;
class QListWidget;
class QPushButton;

class DualShockUDPClientWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit DualShockUDPClientWidget();

signals:
  // Emitted when config has changed so widgets can update to reflect the change.
  void ConfigChanged();

private:
  void CreateWidgets();
  void ConnectWidgets();

  void RefreshServerList();

  void OnServerAdded();
  void OnServerRemoved();
  void OnServersToggled();

  QCheckBox* m_servers_enabled;
  QListWidget* m_server_list;
  QPushButton* m_add_server;
  QPushButton* m_remove_server;
};
