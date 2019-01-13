// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QRadioButton;
class QSlider;
class QVBoxLayout;

class WiiPane : public QWidget
{
  Q_OBJECT
public:
  explicit WiiPane(QWidget* parent = nullptr);
  void OnEmulationStateChanged(bool running);

private:
  void CreateLayout();
  void ConnectLayout();
  void CreateMisc();
  void CreateWiiRemoteSettings();
  void CreateWhitelistedUSBPassthroughDevices();
  void PopulateUSBPassthroughListWidget();

  void OnUSBWhitelistAddButton();
  void OnUSBWhitelistRemoveButton();

  void LoadConfig();
  void OnSaveConfig();

  void ValidateSelectionState();

  // Widgets
  QVBoxLayout* m_main_layout;

  // Misc Settings
  QCheckBox* m_screensaver_checkbox;
  QRadioButton* m_display_mode_pal_50hz_576i;
  QRadioButton* m_display_mode_pal_60hz_480i;
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
