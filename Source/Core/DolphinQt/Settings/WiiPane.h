// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class QLabel;
class QSlider;
class QVBoxLayout;
class QListWidget;
class QPushButton;
class QComboBox;
class QCheckBox;

class WiiPane : public QWidget
{
  Q_OBJECT
public:
  explicit WiiPane(QWidget* parent = nullptr);
  void OnEmulationStateChanged(bool running);

private:
  void PopulateUSBPassthroughListWidget();
  void CreateLayout();
  void ConnectLayout();
  void CreateMisc();
  void CreateWhitelistedUSBPassthroughDevices();
  void CreateWiiRemoteSettings();

  void LoadConfig();
  void OnSaveConfig();

  void ValidateSelectionState();

  void OnUSBWhitelistAddButton();
  void OnUSBWhitelistRemoveButton();

  // Widgets
  QVBoxLayout* m_main_layout;

  // Misc Settings
  QCheckBox* m_screensaver_checkbox;
  QCheckBox* m_pal60_mode_checkbox;
  QCheckBox* m_sd_card_checkbox;
  QCheckBox* m_connect_keyboard_checkbox;
  QComboBox* m_system_language_choice;
  QLabel* m_system_language_choice_label;
  QComboBox* m_aspect_ratio_choice;
  QLabel* m_aspect_ratio_choice_label;

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
