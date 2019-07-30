// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Settings/WiiPane.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QSpacerItem>
#include <QStringList>

#include "Common/Config/Config.h"
#include "Common/StringUtil.h"

#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "DolphinQt/Settings.h"
#include "DolphinQt/Settings/USBDeviceAddToWhitelistDialog.h"

#include "UICommon/USBUtils.h"

// SYSCONF uses 0 for bottom and 1 for top, but we place them in
// the other order in the GUI so that Top will be above Bottom,
// matching the respective physical placements of the sensor bar.
// This also matches the layout of the settings in the Wii Menu.
static int TranslateSensorBarPosition(int position)
{
  if (position == 0)
    return 1;
  if (position == 1)
    return 0;

  return position;
}

WiiPane::WiiPane(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  LoadConfig();
  ConnectLayout();
  ValidateSelectionState();
}

void WiiPane::CreateLayout()
{
  m_main_layout = new QVBoxLayout;
  CreateMisc();
  CreateWhitelistedUSBPassthroughDevices();
  CreateWiiRemoteSettings();
  m_main_layout->addStretch(1);
  setLayout(m_main_layout);
}

void WiiPane::ConnectLayout()
{
  // Misc Settings
  connect(m_aspect_ratio_choice,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
          &WiiPane::OnSaveConfig);
  connect(m_system_language_choice,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
          &WiiPane::OnSaveConfig);
  connect(m_screensaver_checkbox, &QCheckBox::toggled, this, &WiiPane::OnSaveConfig);
  connect(m_pal60_mode_checkbox, &QCheckBox::toggled, this, &WiiPane::OnSaveConfig);
  connect(m_sd_card_checkbox, &QCheckBox::toggled, this, &WiiPane::OnSaveConfig);
  connect(m_connect_keyboard_checkbox, &QCheckBox::toggled, this, &WiiPane::OnSaveConfig);
  connect(&Settings::Instance(), &Settings::SDCardInsertionChanged, m_sd_card_checkbox,
          &QCheckBox::setChecked);
  connect(&Settings::Instance(), &Settings::USBKeyboardConnectionChanged,
          m_connect_keyboard_checkbox, &QCheckBox::setChecked);

  // Whitelisted USB Passthrough Devices
  connect(m_whitelist_usb_list, &QListWidget::itemClicked, this, &WiiPane::ValidateSelectionState);
  connect(m_whitelist_usb_add_button, &QPushButton::clicked, this,
          &WiiPane::OnUSBWhitelistAddButton);
  connect(m_whitelist_usb_remove_button, &QPushButton::pressed, this,
          &WiiPane::OnUSBWhitelistRemoveButton);

  // Wii Remote Settings
  connect(m_wiimote_ir_sensor_position,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
          &WiiPane::OnSaveConfig);
  connect(m_wiimote_ir_sensitivity, &QSlider::valueChanged, this, &WiiPane::OnSaveConfig);
  connect(m_wiimote_speaker_volume, &QSlider::valueChanged, this, &WiiPane::OnSaveConfig);
  connect(m_wiimote_motor, &QCheckBox::toggled, this, &WiiPane::OnSaveConfig);
}

void WiiPane::CreateMisc()
{
  auto* misc_settings_group = new QGroupBox(tr("Misc Settings"));
  auto* misc_settings_group_layout = new QGridLayout();
  misc_settings_group->setLayout(misc_settings_group_layout);
  m_main_layout->addWidget(misc_settings_group);
  m_pal60_mode_checkbox = new QCheckBox(tr("Use PAL60 Mode (EuRGB60)"));
  m_screensaver_checkbox = new QCheckBox(tr("Enable Screen Saver"));
  m_sd_card_checkbox = new QCheckBox(tr("Insert SD Card"));
  m_connect_keyboard_checkbox = new QCheckBox(tr("Connect USB Keyboard"));
  m_aspect_ratio_choice_label = new QLabel(tr("Aspect Ratio:"));
  m_aspect_ratio_choice = new QComboBox();
  m_aspect_ratio_choice->addItem(tr("4:3"));
  m_aspect_ratio_choice->addItem(tr("16:9"));
  m_system_language_choice_label = new QLabel(tr("System Language:"));
  m_system_language_choice = new QComboBox();
  m_system_language_choice->addItem(tr("Japanese"));
  m_system_language_choice->addItem(tr("English"));
  m_system_language_choice->addItem(tr("German"));
  m_system_language_choice->addItem(tr("French"));
  m_system_language_choice->addItem(tr("Spanish"));
  m_system_language_choice->addItem(tr("Italian"));
  m_system_language_choice->addItem(tr("Dutch"));
  m_system_language_choice->addItem(tr("Simplified Chinese"));
  m_system_language_choice->addItem(tr("Traditional Chinese"));
  m_system_language_choice->addItem(tr("Korean"));

  m_pal60_mode_checkbox->setToolTip(tr("Sets the Wii display mode to 60Hz (480i) instead of 50Hz "
                                       "(576i) for PAL games.\nMay not work for all games."));
  m_screensaver_checkbox->setToolTip(tr("Dims the screen after five minutes of inactivity."));
  m_system_language_choice->setToolTip(tr("Sets the Wii system language."));
  m_sd_card_checkbox->setToolTip(tr("Saved to /Wii/sd.raw (default size is 128mb)."));
  m_connect_keyboard_checkbox->setToolTip(tr("May cause slow down in Wii Menu and some games."));

  misc_settings_group_layout->addWidget(m_pal60_mode_checkbox, 0, 0, 1, 1);
  misc_settings_group_layout->addWidget(m_sd_card_checkbox, 0, 1, 1, 1);
  misc_settings_group_layout->addWidget(m_screensaver_checkbox, 1, 0, 1, 1);
  misc_settings_group_layout->addWidget(m_connect_keyboard_checkbox, 1, 1, 1, 1);
  misc_settings_group_layout->addWidget(m_aspect_ratio_choice_label, 2, 0, 1, 1);
  misc_settings_group_layout->addWidget(m_aspect_ratio_choice, 2, 1, 1, 1);
  misc_settings_group_layout->addWidget(m_system_language_choice_label, 3, 0, 1, 1);
  misc_settings_group_layout->addWidget(m_system_language_choice, 3, 1, 1, 1);
}

void WiiPane::CreateWhitelistedUSBPassthroughDevices()
{
  auto* whitelisted_usb_passthrough_devices_group =
      new QGroupBox(tr("Whitelisted USB Passthrough Devices"));
  auto* whitelist_layout = new QGridLayout();
  m_whitelist_usb_list = new QListWidget();
  whitelist_layout->addWidget(m_whitelist_usb_list, 0, 0, 1, -1);
  whitelist_layout->setColumnStretch(0, 1);
  m_whitelist_usb_add_button = new QPushButton(tr("Add..."));
  m_whitelist_usb_remove_button = new QPushButton(tr("Remove"));
  whitelist_layout->addWidget(m_whitelist_usb_add_button, 1, 1);
  whitelist_layout->addWidget(m_whitelist_usb_remove_button, 1, 2);
  whitelist_layout->addWidget(m_whitelist_usb_list, 0, 0);
  whitelisted_usb_passthrough_devices_group->setLayout(whitelist_layout);
  m_main_layout->addWidget(whitelisted_usb_passthrough_devices_group);
}

void WiiPane::CreateWiiRemoteSettings()
{
  auto* wii_remote_settings_group = new QGroupBox(tr("Wii Remote Settings"));
  auto* wii_remote_settings_group_layout = new QGridLayout();
  wii_remote_settings_group->setLayout(wii_remote_settings_group_layout);
  m_main_layout->addWidget(wii_remote_settings_group);
  m_wiimote_motor = new QCheckBox(tr("Wii Remote Rumble"));

  m_wiimote_sensor_position_label = new QLabel(tr("Sensor Bar Position:"));
  m_wiimote_ir_sensor_position = new QComboBox();
  m_wiimote_ir_sensor_position->addItem(tr("Top"));
  m_wiimote_ir_sensor_position->addItem(tr("Bottom"));

  // IR Sensitivity Slider
  // i18n: IR stands for infrared and refers to the pointer functionality of Wii Remotes
  m_wiimote_ir_sensitivity_label = new QLabel(tr("IR Sensitivity:"));
  m_wiimote_ir_sensitivity = new QSlider(Qt::Horizontal);
  m_wiimote_ir_sensitivity->setMinimum(4);
  m_wiimote_ir_sensitivity->setMaximum(127);

  // Speaker Volume Slider
  m_wiimote_speaker_volume_label = new QLabel(tr("Speaker Volume:"));
  m_wiimote_speaker_volume = new QSlider(Qt::Horizontal);
  m_wiimote_speaker_volume->setMinimum(0);
  m_wiimote_speaker_volume->setMaximum(127);

  wii_remote_settings_group_layout->addWidget(m_wiimote_sensor_position_label, 0, 0);
  wii_remote_settings_group_layout->addWidget(m_wiimote_ir_sensor_position, 0, 1);
  wii_remote_settings_group_layout->addWidget(m_wiimote_ir_sensitivity_label, 1, 0);
  wii_remote_settings_group_layout->addWidget(m_wiimote_ir_sensitivity, 1, 1);
  wii_remote_settings_group_layout->addWidget(m_wiimote_speaker_volume_label, 2, 0);
  wii_remote_settings_group_layout->addWidget(m_wiimote_speaker_volume, 2, 1);
  wii_remote_settings_group_layout->addWidget(m_wiimote_motor, 3, 0, 1, -1);
}

void WiiPane::OnEmulationStateChanged(bool running)
{
  m_screensaver_checkbox->setEnabled(!running);
  m_pal60_mode_checkbox->setEnabled(!running);
  m_system_language_choice->setEnabled(!running);
  m_aspect_ratio_choice->setEnabled(!running);
  m_wiimote_motor->setEnabled(!running);
  m_wiimote_speaker_volume->setEnabled(!running);
  m_wiimote_ir_sensitivity->setEnabled(!running);
  m_wiimote_ir_sensor_position->setEnabled(!running);
}

void WiiPane::LoadConfig()
{
  m_screensaver_checkbox->setChecked(Config::Get(Config::SYSCONF_SCREENSAVER));
  m_pal60_mode_checkbox->setChecked(Config::Get(Config::SYSCONF_PAL60));
  m_sd_card_checkbox->setChecked(Settings::Instance().IsSDCardInserted());
  m_connect_keyboard_checkbox->setChecked(Settings::Instance().IsUSBKeyboardConnected());
  m_aspect_ratio_choice->setCurrentIndex(Config::Get(Config::SYSCONF_WIDESCREEN));
  m_system_language_choice->setCurrentIndex(Config::Get(Config::SYSCONF_LANGUAGE));

  PopulateUSBPassthroughListWidget();

  m_wiimote_ir_sensor_position->setCurrentIndex(
      TranslateSensorBarPosition(Config::Get(Config::SYSCONF_SENSOR_BAR_POSITION)));
  m_wiimote_ir_sensitivity->setValue(Config::Get(Config::SYSCONF_SENSOR_BAR_SENSITIVITY));
  m_wiimote_speaker_volume->setValue(Config::Get(Config::SYSCONF_SPEAKER_VOLUME));
  m_wiimote_motor->setChecked(Config::Get(Config::SYSCONF_WIIMOTE_MOTOR));
}

void WiiPane::OnSaveConfig()
{
  Config::ConfigChangeCallbackGuard config_guard;

  Config::SetBase(Config::SYSCONF_SCREENSAVER, m_screensaver_checkbox->isChecked());
  Config::SetBase(Config::SYSCONF_PAL60, m_pal60_mode_checkbox->isChecked());
  Settings::Instance().SetSDCardInserted(m_sd_card_checkbox->isChecked());
  Settings::Instance().SetUSBKeyboardConnected(m_connect_keyboard_checkbox->isChecked());

  Config::SetBase<u32>(Config::SYSCONF_SENSOR_BAR_POSITION,
                       TranslateSensorBarPosition(m_wiimote_ir_sensor_position->currentIndex()));
  Config::SetBase<u32>(Config::SYSCONF_SENSOR_BAR_SENSITIVITY, m_wiimote_ir_sensitivity->value());
  Config::SetBase<u32>(Config::SYSCONF_SPEAKER_VOLUME, m_wiimote_speaker_volume->value());
  Config::SetBase<u32>(Config::SYSCONF_LANGUAGE, m_system_language_choice->currentIndex());
  Config::SetBase<bool>(Config::SYSCONF_WIDESCREEN, m_aspect_ratio_choice->currentIndex());
  Config::SetBase(Config::SYSCONF_WIIMOTE_MOTOR, m_wiimote_motor->isChecked());
}

void WiiPane::ValidateSelectionState()
{
  m_whitelist_usb_remove_button->setEnabled(m_whitelist_usb_list->currentIndex().isValid());
}

void WiiPane::OnUSBWhitelistAddButton()
{
  USBDeviceAddToWhitelistDialog usb_whitelist_dialog(this);
  connect(&usb_whitelist_dialog, &USBDeviceAddToWhitelistDialog::accepted, this,
          &WiiPane::PopulateUSBPassthroughListWidget);
  usb_whitelist_dialog.exec();
}

void WiiPane::OnUSBWhitelistRemoveButton()
{
  QString device = m_whitelist_usb_list->currentItem()->text().left(9);
  QString vid =
      QString(device.split(QString::fromStdString(":"), QString::SplitBehavior::KeepEmptyParts)[0]);
  QString pid =
      QString(device.split(QString::fromStdString(":"), QString::SplitBehavior::KeepEmptyParts)[1]);
  const u16 vid_u16 = static_cast<u16>(std::stoul(vid.toStdString(), nullptr, 16));
  const u16 pid_u16 = static_cast<u16>(std::stoul(pid.toStdString(), nullptr, 16));
  SConfig::GetInstance().m_usb_passthrough_devices.erase({vid_u16, pid_u16});
  PopulateUSBPassthroughListWidget();
}

void WiiPane::PopulateUSBPassthroughListWidget()
{
  m_whitelist_usb_list->clear();
  for (const auto& device : SConfig::GetInstance().m_usb_passthrough_devices)
  {
    QListWidgetItem* usb_lwi =
        new QListWidgetItem(QString::fromStdString(USBUtils::GetDeviceName(device)));
    m_whitelist_usb_list->addItem(usb_lwi);
  }
  ValidateSelectionState();
}
