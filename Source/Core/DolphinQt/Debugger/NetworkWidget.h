// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDockWidget>

#include "Common/CommonTypes.h"

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QGroupBox;
class QPushButton;
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
  QGroupBox* CreateDumpOptionsGroup();
  QGroupBox* CreateSecurityOptionsGroup();
  QComboBox* CreateDumpFormatCombo();

  void OnDumpFormatComboChanged(int index);

  enum class FormatComboId : int
  {
    None = 0,
    PCAP,
    BinarySSL,
    BinarySSLRead,
    BinarySSLWrite,
  };

  QTableWidget* m_socket_table;
  QTableWidget* m_ssl_table;
  QComboBox* m_dump_format_combo;
  QCheckBox* m_dump_ssl_read_checkbox;
  QCheckBox* m_dump_ssl_write_checkbox;
  QCheckBox* m_dump_root_ca_checkbox;
  QCheckBox* m_dump_peer_cert_checkbox;
  QCheckBox* m_verify_certificates_checkbox;
  QCheckBox* m_dump_bba_checkbox;
  QPushButton* m_open_dump_folder;
};
