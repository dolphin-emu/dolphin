// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/WiiPane.h"

#include <array>
#include <future>
#include <optional>
#include <utility>

#include <QDir>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpacerItem>

#include "Common/Config/Config.h"
#include "Common/FatFsUtil.h"
#include "Common/FileUtil.h"

#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/System.h"
#include "Core/USBUtils.h"

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/Config/ConfigControls/ConfigChoice.h"
#include "DolphinQt/Config/ConfigControls/ConfigSlider.h"
#include "DolphinQt/Config/ConfigControls/ConfigUserPath.h"
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/ParallelProgressDialog.h"
#include "DolphinQt/QtUtils/QtUtils.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/Settings/USBDevicePicker.h"

namespace
{
struct SDSizeComboEntry
{
  u64 size;
  const char* name;
};
static constexpr u64 MebibytesToBytes(u64 mebibytes)
{
  return mebibytes * 1024u * 1024u;
}
static constexpr u64 GibibytesToBytes(u64 gibibytes)
{
  return MebibytesToBytes(gibibytes * 1024u);
}
constexpr std::array sd_size_combo_entries{
    SDSizeComboEntry{0, _trans("Auto")},
    SDSizeComboEntry{MebibytesToBytes(64), _trans("64 MiB")},
    SDSizeComboEntry{MebibytesToBytes(128), _trans("128 MiB")},
    SDSizeComboEntry{MebibytesToBytes(256), _trans("256 MiB")},
    SDSizeComboEntry{MebibytesToBytes(512), _trans("512 MiB")},
    SDSizeComboEntry{GibibytesToBytes(1), _trans("1 GiB")},
    SDSizeComboEntry{GibibytesToBytes(2), _trans("2 GiB")},
    SDSizeComboEntry{GibibytesToBytes(4), _trans("4 GiB (SDHC)")},
    SDSizeComboEntry{GibibytesToBytes(8), _trans("8 GiB (SDHC)")},
    SDSizeComboEntry{GibibytesToBytes(16), _trans("16 GiB (SDHC)")},
    SDSizeComboEntry{GibibytesToBytes(32), _trans("32 GiB (SDHC)")},
};
}  // namespace

WiiPane::WiiPane(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  PopulateUSBPassthroughListWidget();
  ConnectLayout();
  ValidateSelectionState();
  OnEmulationStateChanged(!Core::IsUninitialized(Core::System::GetInstance()));
}

void WiiPane::CreateLayout()
{
  m_main_layout = new QVBoxLayout{this};
  CreateMisc();
  CreateSDCard();
  CreateWhitelistedUSBPassthroughDevices();
  CreateWiiRemoteSettings();
}

void WiiPane::ConnectLayout()
{
  // Whitelisted USB Passthrough Devices
  connect(&Settings::Instance(), &Settings::ConfigChanged, this,
          &WiiPane::PopulateUSBPassthroughListWidget);
  connect(m_whitelist_usb_list, &QListWidget::itemClicked, this, &WiiPane::ValidateSelectionState);
  connect(m_whitelist_usb_add_button, &QPushButton::clicked, this,
          &WiiPane::OnUSBWhitelistAddButton);
  connect(m_whitelist_usb_remove_button, &QPushButton::clicked, this,
          &WiiPane::OnUSBWhitelistRemoveButton);

  // Emulation State
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    OnEmulationStateChanged(state != Core::State::Uninitialized);
  });
}

void WiiPane::CreateMisc()
{
  auto* misc_settings_group = new QGroupBox(tr("Misc Settings"));
  auto* misc_settings_group_layout = new QGridLayout();
  misc_settings_group->setLayout(misc_settings_group_layout);
  m_main_layout->addWidget(misc_settings_group);
  m_pal60_mode_checkbox = new ConfigBool(tr("Use PAL60 Mode (EuRGB60)"), Config::SYSCONF_PAL60);
  m_screensaver_checkbox = new ConfigBool(tr("Enable Screen Saver"), Config::SYSCONF_SCREENSAVER);
  m_wiilink_checkbox =
      new ConfigBool(tr("Enable WiiConnect24 via WiiLink"), Config::MAIN_WII_WIILINK_ENABLE);
  m_connect_keyboard_checkbox =
      new ConfigBool(tr("Connect USB Keyboard"), Config::MAIN_WII_KEYBOARD);

  m_aspect_ratio_choice_label = new QLabel(tr("Aspect Ratio:"));
  m_aspect_ratio_choice = new ConfigChoiceMap<bool>({{tr("4:3"), false}, {tr("16:9"), true}},
                                                    Config::SYSCONF_WIDESCREEN);

  m_system_language_choice_label = new QLabel(tr("System Language:"));
  m_system_language_choice = new ConfigChoiceU32(
      {tr("Japanese"), tr("English"), tr("German"), tr("French"), tr("Spanish"), tr("Italian"),
       tr("Dutch"), tr("Simplified Chinese"), tr("Traditional Chinese"), tr("Korean")},
      Config::SYSCONF_LANGUAGE);

  m_sound_mode_choice_label = new QLabel(tr("Sound:"));
  m_sound_mode_choice = new ConfigChoiceU32(
      {// i18n: Mono audio
       tr("Mono"),
       // i18n: Stereo audio
       tr("Stereo"),
       // i18n: Surround audio (Dolby Pro Logic II)
       tr("Surround")},
      Config::SYSCONF_SOUND_MODE);

  m_pal60_mode_checkbox->SetDescription(
      tr("Sets the Wii display mode to 60Hz (480i) instead of 50Hz "
         "(576i) for PAL games.\nMay not work for all games."));
  m_screensaver_checkbox->SetDescription(tr("Dims the screen after five minutes of inactivity."));
  m_wiilink_checkbox->SetDescription(tr(
      "Enables the WiiLink service for WiiConnect24 channels.\nWiiLink is an alternate provider "
      "for the discontinued WiiConnect24 Channels such as the Forecast and Nintendo Channels\nRead "
      "the Terms of Service at: https://www.wiilink24.com/tos"));
  m_system_language_choice->SetDescription(tr("Sets the Wii system language."));
  m_connect_keyboard_checkbox->SetDescription(
      tr("May cause slow down in Wii Menu and some games."));

  misc_settings_group_layout->addWidget(m_pal60_mode_checkbox, 0, 0, 1, 1);
  misc_settings_group_layout->addWidget(m_connect_keyboard_checkbox, 0, 1, 1, 1);
  misc_settings_group_layout->addWidget(m_screensaver_checkbox, 1, 0, 1, 1);
  misc_settings_group_layout->addWidget(m_wiilink_checkbox, 1, 1, 1, 1);
  misc_settings_group_layout->addWidget(m_aspect_ratio_choice_label, 2, 0, 1, 1);
  misc_settings_group_layout->addWidget(m_aspect_ratio_choice, 2, 1, 1, 1);
  misc_settings_group_layout->addWidget(m_system_language_choice_label, 3, 0, 1, 1);
  misc_settings_group_layout->addWidget(m_system_language_choice, 3, 1, 1, 1);
  misc_settings_group_layout->addWidget(m_sound_mode_choice_label, 4, 0, 1, 1);
  misc_settings_group_layout->addWidget(m_sound_mode_choice, 4, 1, 1, 1);
}

void WiiPane::CreateSDCard()
{
  auto* sd_settings_group = new QGroupBox(tr("SD Card Settings"));
  auto* sd_settings_group_layout = new QGridLayout();
  sd_settings_group->setLayout(sd_settings_group_layout);
  m_main_layout->addWidget(sd_settings_group);

  int row = 0;
  m_sd_card_checkbox = new ConfigBool(tr("Insert SD Card"), Config::MAIN_WII_SD_CARD);
  m_sd_card_checkbox->SetDescription(tr("Supports SD and SDHC. Default size is 128 MB."));
  m_allow_sd_writes_checkbox =
      new ConfigBool(tr("Allow Writes to SD Card"), Config::MAIN_ALLOW_SD_WRITES);
  sd_settings_group_layout->addWidget(m_sd_card_checkbox, row, 0, 1, 1);
  sd_settings_group_layout->addWidget(m_allow_sd_writes_checkbox, row, 1, 1, 1);
  ++row;

  {
    QHBoxLayout* hlayout = new QHBoxLayout;
    m_sd_raw_edit = new ConfigUserPath(F_WIISDCARDIMAGE_IDX, Config::MAIN_WII_SD_CARD_IMAGE_PATH);
    QPushButton* sdcard_open = new NonDefaultQPushButton(QStringLiteral("..."));
    connect(sdcard_open, &QPushButton::clicked, this, &WiiPane::BrowseSDRaw);
    hlayout->addWidget(new QLabel(tr("SD Card Path:")));
    hlayout->addWidget(m_sd_raw_edit);
    hlayout->addWidget(sdcard_open);

    sd_settings_group_layout->addLayout(hlayout, row, 0, 1, 2);
    ++row;
  }

  m_sync_sd_folder_checkbox = new ConfigBool(tr("Automatically Sync with Folder"),
                                             Config::MAIN_WII_SD_CARD_ENABLE_FOLDER_SYNC);
  m_sync_sd_folder_checkbox->SetDescription(
      tr("Synchronizes the SD Card with the SD Sync Folder when starting and ending emulation."));
  sd_settings_group_layout->addWidget(m_sync_sd_folder_checkbox, row, 0, 1, 2);
  ++row;

  {
    QHBoxLayout* hlayout = new QHBoxLayout;
    m_sd_sync_folder_edit =
        new ConfigUserPath(D_WIISDCARDSYNCFOLDER_IDX, Config::MAIN_WII_SD_CARD_SYNC_FOLDER_PATH);
    QPushButton* sdcard_open = new NonDefaultQPushButton(QStringLiteral("..."));
    connect(sdcard_open, &QPushButton::clicked, this, &WiiPane::BrowseSDSyncFolder);
    hlayout->addWidget(new QLabel(tr("SD Sync Folder:")));
    hlayout->addWidget(m_sd_sync_folder_edit);
    hlayout->addWidget(sdcard_open);

    sd_settings_group_layout->addLayout(hlayout, row, 0, 1, 2);
    ++row;
  }

  std::vector<std::pair<QString, u64>> sd_size_choices;
  for (const auto& entry : sd_size_combo_entries)
    sd_size_choices.emplace_back(tr(entry.name), entry.size);

  m_sd_card_size_combo = new ConfigChoiceMap(sd_size_choices, Config::MAIN_WII_SD_CARD_FILESIZE);
  sd_settings_group_layout->addWidget(new QLabel(tr("SD Card File Size:")), row, 0);
  sd_settings_group_layout->addWidget(m_sd_card_size_combo, row, 1);
  ++row;

  m_sd_pack_button = new NonDefaultQPushButton(tr(Common::SD_PACK_TEXT));
  m_sd_unpack_button = new NonDefaultQPushButton(tr(Common::SD_UNPACK_TEXT));
  connect(m_sd_pack_button, &QPushButton::clicked, [this] {
    auto result = ModalMessageBox::warning(
        this, tr(Common::SD_PACK_TEXT),
        tr("You are about to pack the content of the folder at %1 into the file at %2. All "
           "current content of the file will be deleted. Are you sure you want to continue?")
            .arg(QString::fromStdString(File::GetUserPath(D_WIISDCARDSYNCFOLDER_IDX)))
            .arg(QString::fromStdString(File::GetUserPath(F_WIISDCARDIMAGE_IDX))),
        QMessageBox::Yes | QMessageBox::No);
    if (result == QMessageBox::Yes)
    {
      ParallelProgressDialog progress_dialog(tr("Converting..."), tr("Cancel"), 0, 0, this);
      progress_dialog.GetRaw()->setWindowModality(Qt::WindowModal);
      progress_dialog.GetRaw()->setWindowTitle(tr("Progress"));
      auto success = std::async(std::launch::async, [&] {
        const bool good = Common::SyncSDFolderToSDImage(
            [&progress_dialog] { return progress_dialog.WasCanceled(); }, false);
        progress_dialog.Reset();
        return good;
      });
      progress_dialog.GetRaw()->exec();
      if (!success.get())
        ModalMessageBox::warning(this, tr(Common::SD_PACK_TEXT), tr("Conversion failed."));
    }
  });
  connect(m_sd_unpack_button, &QPushButton::clicked, [this] {
    auto result = ModalMessageBox::warning(
        this, tr(Common::SD_UNPACK_TEXT),
        tr("You are about to unpack the content of the file at %2 into the folder at %1. All "
           "current content of the folder will be deleted. Are you sure you want to continue?")
            .arg(QString::fromStdString(File::GetUserPath(D_WIISDCARDSYNCFOLDER_IDX)))
            .arg(QString::fromStdString(File::GetUserPath(F_WIISDCARDIMAGE_IDX))),
        QMessageBox::Yes | QMessageBox::No);
    if (result == QMessageBox::Yes)
    {
      ParallelProgressDialog progress_dialog(tr("Converting..."), tr("Cancel"), 0, 0, this);
      progress_dialog.GetRaw()->setWindowModality(Qt::WindowModal);
      progress_dialog.GetRaw()->setWindowTitle(tr("Progress"));
      auto success = std::async(std::launch::async, [&] {
        const bool good = Common::SyncSDImageToSDFolder(
            [&progress_dialog] { return progress_dialog.WasCanceled(); });
        progress_dialog.Reset();
        return good;
      });
      progress_dialog.GetRaw()->exec();
      if (!success.get())
        ModalMessageBox::warning(this, tr(Common::SD_UNPACK_TEXT), tr("Conversion failed."));
    }
  });
  sd_settings_group_layout->addWidget(m_sd_pack_button, row, 0, 1, 1);
  sd_settings_group_layout->addWidget(m_sd_unpack_button, row, 1, 1, 1);
  ++row;
}

void WiiPane::CreateWhitelistedUSBPassthroughDevices()
{
  m_whitelist_usb_list = new QtUtils::MinimumSizeHintWidget<QListWidget>;

  m_whitelist_usb_add_button = new NonDefaultQPushButton(tr("Add..."));
  m_whitelist_usb_remove_button = new NonDefaultQPushButton(tr("Remove"));

  QHBoxLayout* hlayout = new QHBoxLayout;
  hlayout->addStretch();
  hlayout->addWidget(m_whitelist_usb_add_button);
  hlayout->addWidget(m_whitelist_usb_remove_button);

  QVBoxLayout* vlayout = new QVBoxLayout;
  vlayout->addWidget(m_whitelist_usb_list);
  vlayout->addLayout(hlayout);

  auto* whitelisted_usb_passthrough_devices_group =
      new QGroupBox(tr("Whitelisted USB Passthrough Devices"));
  whitelisted_usb_passthrough_devices_group->setLayout(vlayout);

  m_main_layout->addWidget(whitelisted_usb_passthrough_devices_group);
}

void WiiPane::CreateWiiRemoteSettings()
{
  auto* wii_remote_settings_group = new QGroupBox(tr("Wii Remote Settings"));
  auto* wii_remote_settings_group_layout = new QGridLayout();
  wii_remote_settings_group->setLayout(wii_remote_settings_group_layout);
  m_main_layout->addWidget(wii_remote_settings_group);
  m_wiimote_motor = new ConfigBool(tr("Enable Rumble"), Config::SYSCONF_WIIMOTE_MOTOR);

  m_wiimote_sensor_position_label = new QLabel(tr("Sensor Bar Position:"));
  m_wiimote_ir_sensor_position = new ConfigChoiceMap<u32>({{tr("Top"), 1}, {tr("Bottom"), 0}},
                                                          Config::SYSCONF_SENSOR_BAR_POSITION);

  // IR Sensitivity Slider
  // i18n: IR stands for infrared and refers to the pointer functionality of Wii Remotes
  m_wiimote_ir_sensitivity_label = new QLabel(tr("IR Sensitivity:"));
  // Wii menu saves values from 1 to 5.
  m_wiimote_ir_sensitivity = new ConfigSliderU32(1, 5, Config::SYSCONF_SENSOR_BAR_SENSITIVITY, 1);

  // Speaker Volume Slider
  m_wiimote_speaker_volume_label = new QLabel(tr("Speaker Volume:"));
  m_wiimote_speaker_volume = new ConfigSliderU32(0, 127, Config::SYSCONF_SPEAKER_VOLUME, 1);

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
  m_sound_mode_choice->setEnabled(!running);
  m_sd_pack_button->setEnabled(!running);
  m_sd_unpack_button->setEnabled(!running);
  m_wiimote_motor->setEnabled(!running);
  m_wiimote_speaker_volume->setEnabled(!running);
  m_wiimote_ir_sensitivity->setEnabled(!running);
  m_wiimote_ir_sensor_position->setEnabled(!running);
  m_wiilink_checkbox->setEnabled(!running);
}

void WiiPane::ValidateSelectionState()
{
  m_whitelist_usb_remove_button->setEnabled(m_whitelist_usb_list->currentIndex().isValid());
}

void WiiPane::OnUSBWhitelistAddButton()
{
  auto whitelist = Config::GetUSBDeviceWhitelist();

  const std::optional<USBUtils::DeviceInfo> usb_device = USBDevicePicker::Run(
      this, tr("Add New USB Device"),
      [&whitelist](const USBUtils::DeviceInfo& device) { return !whitelist.contains(device); });
  if (!usb_device)
    return;

  if (whitelist.contains(*usb_device))
  {
    ModalMessageBox::critical(this, tr("USB Whitelist Error"),
                              tr("This USB device is already whitelisted."));
    return;
  }
  whitelist.emplace(*usb_device);
  Config::SetUSBDeviceWhitelist(whitelist);
  Config::Save();
  PopulateUSBPassthroughListWidget();
}

void WiiPane::OnUSBWhitelistRemoveButton()
{
  auto* current_item = m_whitelist_usb_list->currentItem();
  if (!current_item)
    return;

  QVariant item_data = current_item->data(Qt::UserRole);
  USBUtils::DeviceInfo device = item_data.value<USBUtils::DeviceInfo>();

  auto whitelist = Config::GetUSBDeviceWhitelist();
  whitelist.erase(device);
  Config::SetUSBDeviceWhitelist(whitelist);
  PopulateUSBPassthroughListWidget();
}

void WiiPane::PopulateUSBPassthroughListWidget()
{
  m_whitelist_usb_list->clear();
  auto whitelist = Config::GetUSBDeviceWhitelist();
  for (auto& device : whitelist)
  {
    auto* item =
        new QListWidgetItem(QString::fromStdString(device.ToDisplayString()), m_whitelist_usb_list);
    QVariant device_data = QVariant::fromValue(device);
    item->setData(Qt::UserRole, device_data);
  }
  ValidateSelectionState();
}

void WiiPane::BrowseSDRaw()
{
  QString file = QDir::toNativeSeparators(DolphinFileDialog::getOpenFileName(
      this, tr("Select SD Card Image"),
      QString::fromStdString(Config::Get(Config::MAIN_WII_SD_CARD_IMAGE_PATH)),
      tr("SD Card Image (*.raw);;"
         "All Files (*)")));
  if (!file.isEmpty())
    m_sd_raw_edit->SetTextAndUpdate(file);
}

void WiiPane::BrowseSDSyncFolder()
{
  QString file = QDir::toNativeSeparators(DolphinFileDialog::getExistingDirectory(
      this, tr("Select a Folder to Sync with the SD Card Image"),
      QString::fromStdString(File::GetUserPath(D_WIISDCARDSYNCFOLDER_IDX))));
  if (!file.isEmpty())
    m_sd_sync_folder_edit->SetTextAndUpdate(file);
}
