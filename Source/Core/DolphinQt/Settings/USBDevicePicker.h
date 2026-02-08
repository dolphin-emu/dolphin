// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <vector>

#include <QDialog>

#include "Core/USBUtils.h"

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

class USBDevicePicker final : public QDialog
{
  Q_OBJECT
public:
  using FilterFunctionType = std::function<bool(const USBUtils::DeviceInfo&)>;
  explicit USBDevicePicker(QWidget* parent, FilterFunctionType filter);

  static std::optional<USBUtils::DeviceInfo> Run(
      QWidget* parent, const QString& title,
      const FilterFunctionType filter = [](const USBUtils::DeviceInfo&) { return true; });

private:
  static constexpr int DEVICE_REFRESH_INTERVAL_MS = 100;
  QTimer* m_refresh_devices_timer;
  QDialogButtonBox* m_picker_buttonbox;
  QVBoxLayout* main_layout;
  QLabel* enter_device_id_label;
  QHBoxLayout* entry_hbox_layout;
  QLineEdit* device_vid_textbox;
  QLineEdit* device_pid_textbox;
  QLabel* select_label;
  QListWidget* usb_inserted_devices_list;

  FilterFunctionType m_filter;

  std::optional<USBUtils::DeviceInfo> GetSelectedDevice() const;

  void InitControls();
  void RefreshDeviceList();

  void OnDeviceSelection();

  std::vector<USBUtils::DeviceInfo> m_shown_devices;
};
