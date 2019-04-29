// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Settings/USBDeviceAddToWhitelistDialog.h"

#include <QButtonGroup>
#include <QDialog>
#include <QDialogButtonBox>
#include <QErrorMessage>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Settings/WiiPane.h"

#include "UICommon/USBUtils.h"

static bool IsValidUSBIDString(const std::string& string)
{
  if (string.empty() || string.length() > 4)
    return false;
  return std::all_of(string.begin(), string.end(),
                     [](const auto character) { return std::isxdigit(character) != 0; });
}

USBDeviceAddToWhitelistDialog::USBDeviceAddToWhitelistDialog(QWidget* parent) : QDialog(parent)
{
  InitControls();
  setLayout(main_layout);
}

void USBDeviceAddToWhitelistDialog::InitControls()
{
  setWindowTitle(tr("Add New USB Device"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  m_whitelist_buttonbox = new QDialogButtonBox();
  auto* add_button = new QPushButton(tr("Add"));
  auto* cancel_button = new QPushButton(tr("Cancel"));
  m_whitelist_buttonbox->addButton(add_button, QDialogButtonBox::AcceptRole);
  m_whitelist_buttonbox->addButton(cancel_button, QDialogButtonBox::RejectRole);
  connect(add_button, &QPushButton::clicked, this,
          &USBDeviceAddToWhitelistDialog::AddUSBDeviceToWhitelist);
  connect(cancel_button, &QPushButton::clicked, this, &USBDeviceAddToWhitelistDialog::reject);
  add_button->setDefault(true);

  main_layout = new QVBoxLayout();
  enter_device_id_label = new QLabel(tr("Enter USB device ID"));
  enter_device_id_label->setAlignment(Qt::AlignCenter);
  main_layout->addWidget(enter_device_id_label);

  entry_hbox_layout = new QHBoxLayout();
  device_vid_textbox = new QLineEdit();
  QSizePolicy sizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
  sizePolicy.setHorizontalStretch(1);
  sizePolicy.setVerticalStretch(0);
  sizePolicy.setHeightForWidth(device_vid_textbox->sizePolicy().hasHeightForWidth());
  device_vid_textbox->setSizePolicy(sizePolicy);

  // entry_hbox_layout->setWidget(2, QFormLayout::LabelRole, device_vid_textbox);
  entry_hbox_layout->addWidget(device_vid_textbox);

  device_pid_textbox = new QLineEdit();
  sizePolicy.setHeightForWidth(device_pid_textbox->sizePolicy().hasHeightForWidth());
  device_pid_textbox->setSizePolicy(sizePolicy);

  entry_hbox_layout->addWidget(device_pid_textbox);
  main_layout->addLayout(entry_hbox_layout);

  select_label = new QLabel(tr("or select a device"));
  select_label->setAlignment(Qt::AlignCenter);

  main_layout->addWidget(select_label);

  usb_inserted_devices_list = new QListWidget();
  m_refresh_devices_timer = new QTimer(this);
  connect(usb_inserted_devices_list, &QListWidget::currentItemChanged, this,
          &USBDeviceAddToWhitelistDialog::OnDeviceSelection);
  connect(usb_inserted_devices_list, &QListWidget::itemDoubleClicked, add_button,
          &QPushButton::clicked);
  connect(m_refresh_devices_timer, &QTimer::timeout, this,
          &USBDeviceAddToWhitelistDialog::RefreshDeviceList);
  m_refresh_devices_timer->start(1000);

  main_layout->addWidget(usb_inserted_devices_list);
  main_layout->addWidget(m_whitelist_buttonbox);

  // i18n: VID means Vendor ID (in the context of a USB device)
  device_vid_textbox->setPlaceholderText(tr("Device VID (e.g., 057e)"));
  // i18n: PID means Product ID (in the context of a USB device), not Process ID
  device_pid_textbox->setPlaceholderText(tr("Device PID (e.g., 0305)"));
}

void USBDeviceAddToWhitelistDialog::RefreshDeviceList()
{
  const auto& current_devices = USBUtils::GetInsertedDevices();
  if (current_devices == m_shown_devices)
    return;
  const auto selection_string = usb_inserted_devices_list->currentItem();
  usb_inserted_devices_list->clear();
  for (const auto& device : current_devices)
  {
    if (SConfig::GetInstance().IsUSBDeviceWhitelisted(device.first))
      continue;
    usb_inserted_devices_list->addItem(QString::fromStdString(device.second));
  }

  usb_inserted_devices_list->setCurrentItem(selection_string);

  m_shown_devices = current_devices;
}

void USBDeviceAddToWhitelistDialog::AddUSBDeviceToWhitelist()
{
  const std::string vid_string = StripSpaces(device_vid_textbox->text().toStdString());
  const std::string pid_string = StripSpaces(device_pid_textbox->text().toStdString());
  if (!IsValidUSBIDString(vid_string))
  {
    // i18n: Here, VID means Vendor ID (for a USB device).
    ModalMessageBox vid_warning_box(this);
    vid_warning_box.setIcon(QMessageBox::Warning);
    vid_warning_box.setWindowTitle(tr("USB Whitelist Error"));
    // i18n: Here, VID means Vendor ID (for a USB device).
    vid_warning_box.setText(tr("The entered VID is invalid."));
    vid_warning_box.setStandardButtons(QMessageBox::Ok);
    vid_warning_box.exec();
    return;
  }
  if (!IsValidUSBIDString(pid_string))
  {
    // i18n: Here, PID means Product ID (for a USB device).
    ModalMessageBox pid_warning_box(this);
    pid_warning_box.setIcon(QMessageBox::Warning);
    pid_warning_box.setWindowTitle(tr("USB Whitelist Error"));
    // i18n: Here, PID means Product ID (for a USB device).
    pid_warning_box.setText(tr("The entered PID is invalid."));
    pid_warning_box.setStandardButtons(QMessageBox::Ok);
    pid_warning_box.exec();
    return;
  }

  const u16 vid = static_cast<u16>(std::stoul(vid_string, nullptr, 16));
  const u16 pid = static_cast<u16>(std::stoul(pid_string, nullptr, 16));

  if (SConfig::GetInstance().IsUSBDeviceWhitelisted({vid, pid}))
  {
    ModalMessageBox::critical(this, tr("Error"), tr("This USB device is already whitelisted."));
    return;
  }
  SConfig::GetInstance().m_usb_passthrough_devices.emplace(vid, pid);
  SConfig::GetInstance().SaveSettings();
  accept();
}

void USBDeviceAddToWhitelistDialog::OnDeviceSelection()
{
  // Not the nicest way of doing this but...
  QString device = usb_inserted_devices_list->currentItem()->text().left(9);
  QString* vid = new QString(
      device.split(QString::fromStdString(":"), QString::SplitBehavior::KeepEmptyParts)[0]);
  QString* pid = new QString(
      device.split(QString::fromStdString(":"), QString::SplitBehavior::KeepEmptyParts)[1]);
  device_vid_textbox->setText(*vid);
  device_pid_textbox->setText(*pid);
}
