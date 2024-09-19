// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/WiiPane.h"

#include <array>
#include <future>
#include <utility>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSlider>
#include <QStringList>

#include "Common/Config/Config.h"
#include "Common/FatFsUtil.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/System.h"

#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/ParallelProgressDialog.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
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
  LoadConfig();
  ConnectLayout();
  ValidateSelectionState();
  OnEmulationStateChanged(Core::GetState(Core::System::GetInstance()) !=
                          Core::State::Uninitialized);
}

void WiiPane::CreateLayout()
{
  m_main_layout = new QVBoxLayout;
  CreateMisc();
  CreateSDCard();
  CreateWhitelistedUSBPassthroughDevices();
  CreateWiiRemoteSettings();
  m_main_layout->addStretch(1);
  setLayout(m_main_layout);
}

void WiiPane::ConnectLayout()
{
  // Misc Settings
  connect(m_aspect_ratio_choice, &QComboBox::currentIndexChanged, this, &WiiPane::OnSaveConfig);
  connect(m_system_language_choice, &QComboBox::currentIndexChanged, this, &WiiPane::OnSaveConfig);
  connect(m_sound_mode_choice, &QComboBox::currentIndexChanged, this, &WiiPane::OnSaveConfig);
  connect(m_screensaver_checkbox, &QCheckBox::toggled, this, &WiiPane::OnSaveConfig);
  connect(m_pal60_mode_checkbox, &QCheckBox::toggled, this, &WiiPane::OnSaveConfig);
  connect(m_connect_keyboard_checkbox, &QCheckBox::toggled, this, &WiiPane::OnSaveConfig);
  connect(&Settings::Instance(), &Settings::SDCardInsertionChanged, m_sd_card_checkbox,
          &QCheckBox::setChecked);
  connect(&Settings::Instance(), &Settings::USBKeyboardConnectionChanged,
          m_connect_keyboard_checkbox, &QCheckBox::setChecked);
  connect(m_wiilink_checkbox, &QCheckBox::toggled, this, &WiiPane::OnSaveConfig);

  // SD Card Settings
  connect(m_sd_card_checkbox, &QCheckBox::toggled, this, &WiiPane::OnSaveConfig);
  connect(m_allow_sd_writes_checkbox, &QCheckBox::toggled, this, &WiiPane::OnSaveConfig);
  connect(m_sync_sd_folder_checkbox, &QCheckBox::toggled, this, &WiiPane::OnSaveConfig);
  connect(m_sd_card_size_combo, &QComboBox::currentIndexChanged, this, &WiiPane::OnSaveConfig);

  // Whitelisted USB Passthrough Devices
  connect(m_whitelist_usb_list, &QListWidget::itemClicked, this, &WiiPane::ValidateSelectionState);
  connect(m_whitelist_usb_add_button, &QPushButton::clicked, this,
          &WiiPane::OnUSBWhitelistAddButton);
  connect(m_whitelist_usb_remove_button, &QPushButton::clicked, this,
          &WiiPane::OnUSBWhitelistRemoveButton);

  // Wii Remote Settings
  connect(m_wiimote_ir_sensor_position, &QComboBox::currentIndexChanged, this,
          &WiiPane::OnSaveConfig);
  connect(m_wiimote_ir_sensitivity, &QSlider::valueChanged, this, &WiiPane::OnSaveConfig);
  connect(m_wiimote_speaker_volume, &QSlider::valueChanged, this, &WiiPane::OnSaveConfig);
  connect(m_wiimote_motor, &QCheckBox::toggled, this, &WiiPane::OnSaveConfig);

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
  m_pal60_mode_checkbox = new QCheckBox(tr("Use PAL60 Mode (EuRGB60)"));
  m_screensaver_checkbox = new QCheckBox(tr("Enable Screen Saver"));
  m_wiilink_checkbox = new QCheckBox(tr("Enable WiiConnect24 via WiiLink"));
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

  m_sound_mode_choice_label = new QLabel(tr("Sound:"));
  m_sound_mode_choice = new QComboBox();
  m_sound_mode_choice->addItem(tr("Mono"));
  m_sound_mode_choice->addItem(tr("Stereo"));
  // i18n: Surround audio (Dolby Pro Logic II)
  m_sound_mode_choice->addItem(tr("Surround"));

  m_pal60_mode_checkbox->setToolTip(tr("Sets the Wii display mode to 60Hz (480i) instead of 50Hz "
                                       "(576i) for PAL games.\nMay not work for all games."));
  m_screensaver_checkbox->setToolTip(tr("Dims the screen after five minutes of inactivity."));
  m_wiilink_checkbox->setToolTip(tr(
      "Enables the WiiLink service for WiiConnect24 channels.\nWiiLink is an alternate provider "
      "for the discontinued WiiConnect24 Channels such as the Forecast and Nintendo Channels\nRead "
      "the Terms of Service at: https://www.wiilink24.com/tos"));
  m_system_language_choice->setToolTip(tr("Sets the Wii system language."));
  m_connect_keyboard_checkbox->setToolTip(tr("May cause slow down in Wii Menu and some games."));

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
  m_sd_card_checkbox = new QCheckBox(tr("Insert SD Card"));
  m_sd_card_checkbox->setToolTip(tr("Supports SD and SDHC. Default size is 128 MB."));
  m_allow_sd_writes_checkbox = new QCheckBox(tr("Allow Writes to SD Card"));
  sd_settings_group_layout->addWidget(m_sd_card_checkbox, row, 0, 1, 1);
  sd_settings_group_layout->addWidget(m_allow_sd_writes_checkbox, row, 1, 1, 1);
  ++row;

  {
    QHBoxLayout* hlayout = new QHBoxLayout;
    m_sd_raw_edit = new QLineEdit(QString::fromStdString(File::GetUserPath(F_WIISDCARDIMAGE_IDX)));
    connect(m_sd_raw_edit, &QLineEdit::editingFinished,
            [this] { SetSDRaw(m_sd_raw_edit->text()); });
    QPushButton* sdcard_open = new NonDefaultQPushButton(QStringLiteral("..."));
    connect(sdcard_open, &QPushButton::clicked, this, &WiiPane::BrowseSDRaw);
    hlayout->addWidget(new QLabel(tr("SD Card Path:")));
    hlayout->addWidget(m_sd_raw_edit);
    hlayout->addWidget(sdcard_open);

    sd_settings_group_layout->addLayout(hlayout, row, 0, 1, 2);
    ++row;
  }

  m_sync_sd_folder_checkbox = new QCheckBox(tr("Automatically Sync with Folder"));
  m_sync_sd_folder_checkbox->setToolTip(
      tr("Synchronizes the SD Card with the SD Sync Folder when starting and ending emulation."));
  sd_settings_group_layout->addWidget(m_sync_sd_folder_checkbox, row, 0, 1, 2);
  ++row;

  {
    QHBoxLayout* hlayout = new QHBoxLayout;
    m_sd_sync_folder_edit =
        new QLineEdit(QString::fromStdString(File::GetUserPath(D_WIISDCARDSYNCFOLDER_IDX)));
    connect(m_sd_sync_folder_edit, &QLineEdit::editingFinished,
            [this] { SetSDSyncFolder(m_sd_sync_folder_edit->text()); });
    QPushButton* sdcard_open = new NonDefaultQPushButton(QStringLiteral("..."));
    connect(sdcard_open, &QPushButton::clicked, this, &WiiPane::BrowseSDSyncFolder);
    hlayout->addWidget(new QLabel(tr("SD Sync Folder:")));
    hlayout->addWidget(m_sd_sync_folder_edit);
    hlayout->addWidget(sdcard_open);

    sd_settings_group_layout->addLayout(hlayout, row, 0, 1, 2);
    ++row;
  }

  m_sd_card_size_combo = new QComboBox();
  for (size_t i = 0; i < sd_size_combo_entries.size(); ++i)
    m_sd_card_size_combo->addItem(tr(sd_size_combo_entries[i].name));
  sd_settings_group_layout->addWidget(new QLabel(tr("SD Card File Size:")), row, 0);
  sd_settings_group_layout->addWidget(m_sd_card_size_combo, row, 1);
  ++row;

  m_sd_pack_button = new NonDefaultQPushButton(tr("Convert Folder to File Now"));
  m_sd_unpack_button = new NonDefaultQPushButton(tr("Convert File to Folder Now"));
  connect(m_sd_pack_button, &QPushButton::clicked, [this] {
    auto result = ModalMessageBox::warning(
        this, tr("Convert Folder to File Now"),
        tr("You are about to convert the content of the folder at %1 into the file at %2. All "
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
            [&progress_dialog]() { return progress_dialog.WasCanceled(); }, false);
        progress_dialog.Reset();
        return good;
      });
      SetQWidgetWindowDecorations(progress_dialog.GetRaw());
      progress_dialog.GetRaw()->exec();
      if (!success.get())
        ModalMessageBox::warning(this, tr("Convert Folder to File Now"), tr("Conversion failed."));
    }
  });
  connect(m_sd_unpack_button, &QPushButton::clicked, [this] {
    auto result = ModalMessageBox::warning(
        this, tr("Convert File to Folder Now"),
        tr("You are about to convert the content of the file at %2 into the folder at %1. All "
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
            [&progress_dialog]() { return progress_dialog.WasCanceled(); });
        progress_dialog.Reset();
        return good;
      });
      SetQWidgetWindowDecorations(progress_dialog.GetRaw());
      progress_dialog.GetRaw()->exec();
      if (!success.get())
        ModalMessageBox::warning(this, tr("Convert File to Folder Now"), tr("Conversion failed."));
    }
  });
  sd_settings_group_layout->addWidget(m_sd_pack_button, row, 0, 1, 1);
  sd_settings_group_layout->addWidget(m_sd_unpack_button, row, 1, 1, 1);
  ++row;
}

void WiiPane::CreateWhitelistedUSBPassthroughDevices()
{
  m_whitelist_usb_list = new QListWidget();
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
  m_wiimote_motor = new QCheckBox(tr("Enable Rumble"));

  m_wiimote_sensor_position_label = new QLabel(tr("Sensor Bar Position:"));
  m_wiimote_ir_sensor_position = new QComboBox();
  m_wiimote_ir_sensor_position->addItem(tr("Top"));
  m_wiimote_ir_sensor_position->addItem(tr("Bottom"));

  // IR Sensitivity Slider
  // i18n: IR stands for infrared and refers to the pointer functionality of Wii Remotes
  m_wiimote_ir_sensitivity_label = new QLabel(tr("IR Sensitivity:"));
  m_wiimote_ir_sensitivity = new QSlider(Qt::Horizontal);
  // Wii menu saves values from 1 to 5.
  m_wiimote_ir_sensitivity->setMinimum(1);
  m_wiimote_ir_sensitivity->setMaximum(5);

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
  m_sound_mode_choice->setEnabled(!running);
  m_sd_pack_button->setEnabled(!running);
  m_sd_unpack_button->setEnabled(!running);
  m_wiimote_motor->setEnabled(!running);
  m_wiimote_speaker_volume->setEnabled(!running);
  m_wiimote_ir_sensitivity->setEnabled(!running);
  m_wiimote_ir_sensor_position->setEnabled(!running);
  m_wiilink_checkbox->setEnabled(!running);
}

void WiiPane::LoadConfig()
{
  m_screensaver_checkbox->setChecked(Config::Get(Config::SYSCONF_SCREENSAVER));
  m_pal60_mode_checkbox->setChecked(Config::Get(Config::SYSCONF_PAL60));
  m_connect_keyboard_checkbox->setChecked(Settings::Instance().IsUSBKeyboardConnected());
  m_aspect_ratio_choice->setCurrentIndex(Config::Get(Config::SYSCONF_WIDESCREEN));
  m_system_language_choice->setCurrentIndex(Config::Get(Config::SYSCONF_LANGUAGE));
  m_sound_mode_choice->setCurrentIndex(Config::Get(Config::SYSCONF_SOUND_MODE));
  m_wiilink_checkbox->setChecked(Config::Get(Config::MAIN_WII_WIILINK_ENABLE));

  m_sd_card_checkbox->setChecked(Settings::Instance().IsSDCardInserted());
  m_allow_sd_writes_checkbox->setChecked(Config::Get(Config::MAIN_ALLOW_SD_WRITES));
  m_sync_sd_folder_checkbox->setChecked(Config::Get(Config::MAIN_WII_SD_CARD_ENABLE_FOLDER_SYNC));

  const u64 sd_card_size = Config::Get(Config::MAIN_WII_SD_CARD_FILESIZE);
  for (size_t i = 0; i < sd_size_combo_entries.size(); ++i)
  {
    if (sd_size_combo_entries[i].size == sd_card_size)
      m_sd_card_size_combo->setCurrentIndex(static_cast<int>(i));
  }

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
  Settings::Instance().SetUSBKeyboardConnected(m_connect_keyboard_checkbox->isChecked());

  Config::SetBase<u32>(Config::SYSCONF_SENSOR_BAR_POSITION,
                       TranslateSensorBarPosition(m_wiimote_ir_sensor_position->currentIndex()));
  Config::SetBase<u32>(Config::SYSCONF_SENSOR_BAR_SENSITIVITY, m_wiimote_ir_sensitivity->value());
  Config::SetBase<u32>(Config::SYSCONF_SPEAKER_VOLUME, m_wiimote_speaker_volume->value());
  Config::SetBase<u32>(Config::SYSCONF_LANGUAGE, m_system_language_choice->currentIndex());
  Config::SetBase<bool>(Config::SYSCONF_WIDESCREEN, m_aspect_ratio_choice->currentIndex());
  Config::SetBase<u32>(Config::SYSCONF_SOUND_MODE, m_sound_mode_choice->currentIndex());
  Config::SetBase(Config::SYSCONF_WIIMOTE_MOTOR, m_wiimote_motor->isChecked());
  Config::SetBase(Config::MAIN_WII_WIILINK_ENABLE, m_wiilink_checkbox->isChecked());

  Settings::Instance().SetSDCardInserted(m_sd_card_checkbox->isChecked());
  Config::SetBase(Config::MAIN_ALLOW_SD_WRITES, m_allow_sd_writes_checkbox->isChecked());
  Config::SetBase(Config::MAIN_WII_SD_CARD_ENABLE_FOLDER_SYNC,
                  m_sync_sd_folder_checkbox->isChecked());

  const int sd_card_size_index = m_sd_card_size_combo->currentIndex();
  if (sd_card_size_index >= 0 &&
      static_cast<size_t>(sd_card_size_index) < sd_size_combo_entries.size())
  {
    Config::SetBase(Config::MAIN_WII_SD_CARD_FILESIZE,
                    sd_size_combo_entries[sd_card_size_index].size);
  }
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
  SetQWidgetWindowDecorations(&usb_whitelist_dialog);
  usb_whitelist_dialog.exec();
}

void WiiPane::OnUSBWhitelistRemoveButton()
{
  QString device = m_whitelist_usb_list->currentItem()->text().left(9);
  QStringList split = device.split(QString::fromStdString(":"));
  QString vid = QString(split[0]);
  QString pid = QString(split[1]);
  const u16 vid_u16 = static_cast<u16>(std::stoul(vid.toStdString(), nullptr, 16));
  const u16 pid_u16 = static_cast<u16>(std::stoul(pid.toStdString(), nullptr, 16));
  auto whitelist = Config::GetUSBDeviceWhitelist();
  whitelist.erase({vid_u16, pid_u16});
  Config::SetUSBDeviceWhitelist(whitelist);
  PopulateUSBPassthroughListWidget();
}

void WiiPane::PopulateUSBPassthroughListWidget()
{
  m_whitelist_usb_list->clear();
  auto whitelist = Config::GetUSBDeviceWhitelist();
  for (const auto& device : whitelist)
  {
    QListWidgetItem* usb_lwi =
        new QListWidgetItem(QString::fromStdString(USBUtils::GetDeviceName(device)));
    m_whitelist_usb_list->addItem(usb_lwi);
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
    SetSDRaw(file);
}

void WiiPane::SetSDRaw(const QString& path)
{
  Config::SetBase(Config::MAIN_WII_SD_CARD_IMAGE_PATH, path.toStdString());
  SignalBlocking(m_sd_raw_edit)->setText(path);
}

void WiiPane::BrowseSDSyncFolder()
{
  QString file = QDir::toNativeSeparators(DolphinFileDialog::getExistingDirectory(
      this, tr("Select a Folder to Sync with the SD Card Image"),
      QString::fromStdString(Config::Get(Config::MAIN_WII_SD_CARD_SYNC_FOLDER_PATH))));
  if (!file.isEmpty())
    SetSDSyncFolder(file);
}

void WiiPane::SetSDSyncFolder(const QString& path)
{
  Config::SetBase(Config::MAIN_WII_SD_CARD_SYNC_FOLDER_PATH, path.toStdString());
  SignalBlocking(m_sd_sync_folder_edit)->setText(path);
}
