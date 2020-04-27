// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDockWidget>

#include "Common/CommonTypes.h"

class QCheckBox;
class QCloseEvent;
class QGroupBox;
class QShowEvent;
class QTableWidget;
class QTableWidgetItem;

class NetworkWidget : public QDockWidget
{
  Q_OBJECT
public:
  explicit NetworkWidget(QWidget* parent = nullptr);
  ~NetworkWidget();

protected:
  void closeEvent(QCloseEvent*) override;
  void showEvent(QShowEvent* event) override;

private:
  void CreateWidgets();
  void ConnectWidgets();

  void Update();

  QGroupBox* CreateSocketTableGroup();
  QGroupBox* CreateSSLContextGroup();
  QGroupBox* CreateSSLOptionsGroup();

  QTableWidget* m_socket_table;
  QTableWidget* m_ssl_table;
  QCheckBox* m_dump_ssl_read_checkbox;
  QCheckBox* m_dump_ssl_write_checkbox;
  QCheckBox* m_dump_root_ca_checkbox;
  QCheckBox* m_dump_peer_cert_checkbox;
  QCheckBox* m_verify_certificates_checkbox;
};
