// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <QDialog>
#include <QStringList>

#include "Common/SCSIDevice.h"

class OpticalDriveWatcher;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;

// Asks whether to change to a disc image or to the disc in an optical drive. Only used on
// platforms where Common::SCSIDevice::IsSupported() returns true
class ChangeDiscDialog final : public QDialog
{
  Q_OBJECT

public:
  ChangeDiscDialog(std::optional<std::string> current_drive,
                   std::function<QStringList()> prompt_file_names,
                   std::function<void()> eject_current_disc, QWidget* parent = nullptr);

  bool IsOpticalDiscSelected() const;
  std::string GetSelectedDrive() const;
  const std::vector<std::string>& GetSelectedFiles() const;

private:
  void CreateWidgets();
  void ConnectWidgets();
  void UpdateDrive(int index);
  void UpdateStatusWatch();
  void UpdateControls();
  void BrowseFiles();
  void EjectSelectedDrive();

  static QString GetDriveStatusText(const std::optional<Common::OpticalDriveStatus>& status);

  std::optional<std::string> m_current_drive;
  std::function<QStringList()> m_prompt_file_names;
  std::function<void()> m_eject_current_disc;
  std::vector<std::string> m_files;

  OpticalDriveWatcher* m_watcher;
  QRadioButton* m_disc_image_radio;
  QLineEdit* m_file_edit;
  QPushButton* m_browse_button;
  QRadioButton* m_optical_disc_radio;
  QComboBox* m_drive_combo;
  QPushButton* m_eject_button;
  QLabel* m_drive_status_label;
  QDialogButtonBox* m_button_box;
};
