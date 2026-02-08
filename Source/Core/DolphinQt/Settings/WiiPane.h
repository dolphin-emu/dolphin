// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSlider;
class QString;
class QVBoxLayout;

class WiiPane : public QWidget
{
  Q_OBJECT
public:
  explicit WiiPane(QWidget* parent = nullptr);

private:
  void PopulateUSBPassthroughListWidget();
  void CreateLayout();
  void ConnectLayout();
  void CreateMisc();
  void CreateSDCard();
  void CreateWhitelistedUSBPassthroughDevices();
  void CreateWiiRemoteSettings();

  void LoadConfig();
  void OnSaveConfig();
  void OnEmulationStateChanged(bool running);

  void ValidateSelectionState();

  void OnUSBWhitelistAddButton();
  void OnUSBWhitelistRemoveButton();

  void BrowseSDRaw();
  void SetSDRaw(const QString& path);
  void BrowseSDSyncFolder();
  void SetSDSyncFolder(const QString& path);

  // Widgets
  QVBoxLayout* m_main_layout;

  // Misc Settings
  QCheckBox* m_screensaver_checkbox;
  QCheckBox* m_pal60_mode_checkbox;
  QCheckBox* m_connect_keyboard_checkbox;
  QCheckBox* m_wiilink_checkbox;
  QComboBox* m_system_language_choice;
  QLabel* m_system_language_choice_label;
  QComboBox* m_aspect_ratio_choice;
  QLabel* m_aspect_ratio_choice_label;
  QComboBox* m_sound_mode_choice;
  QLabel* m_sound_mode_choice_label;

  // SD Card Settings
  QCheckBox* m_sd_card_checkbox;
  QCheckBox* m_allow_sd_writes_checkbox;
  QCheckBox* m_sync_sd_folder_checkbox;
  QComboBox* m_sd_card_size_combo;
  QLineEdit* m_sd_raw_edit;
  QLineEdit* m_sd_sync_folder_edit;
  QPushButton* m_sd_pack_button;
  QPushButton* m_sd_unpack_button;

  // Whitelisted USB Passthrough Devices
  QListWidget* m_whitelist_usb_list;
  QPushButton* m_whitelist_usb_add_button;
  QPushButton* m_whitelist_usb_remove_button;

  // Wii Remote Settings
  QLabel* m_wiimote_sensor_position_label;
  QComboBox* m_wiimote_ir_sensor_position;
  QSlider* m_wiimote_ir_sensitivity;
  QLabel* m_wiimote_ir_sensitivity_label;
  QSlider* m_wiimote_speaker_volume;
  QLabel* m_wiimote_speaker_volume_label;
  QCheckBox* m_wiimote_motor;
};
