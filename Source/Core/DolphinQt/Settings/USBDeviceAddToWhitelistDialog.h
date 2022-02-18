// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QTimer;
class QDialog;
class QHeaderView;
class QLabel;
class QLineEdit;
class QDialogButtonBox;
class QVBoxLayout;
class QHBoxLayout;
class QListView;
class QPushButton;
class QListWidget;
class QPushButton;
class QErrorMessage;
class QMessageBox;

class USBDeviceAddToWhitelistDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit USBDeviceAddToWhitelistDialog(QWidget* parent);

private:
  static constexpr int DEVICE_REFRESH_INTERVAL_MS = 100;
  QTimer* m_refresh_devices_timer;
  QDialogButtonBox* m_whitelist_buttonbox;
  QVBoxLayout* main_layout;
  QLabel* enter_device_id_label;
  QHBoxLayout* entry_hbox_layout;
  QLineEdit* device_vid_textbox;
  QLineEdit* device_pid_textbox;
  QLabel* select_label;
  QListWidget* usb_inserted_devices_list;

  void InitControls();
  void RefreshDeviceList();
  void AddUSBDeviceToWhitelist();

  void OnDeviceSelection();

  std::map<std::pair<quint16, quint16>, std::string> m_shown_devices;
};
