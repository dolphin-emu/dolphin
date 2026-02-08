// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/USBDevicePicker.h"

#include <optional>

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

#include <fmt/format.h>

#include "Core/USBUtils.h"

#include "DolphinQt/Settings/WiiPane.h"

USBDevicePicker::USBDevicePicker(QWidget* parent, FilterFunctionType filter)
    : QDialog(parent), m_filter(std::move(filter))
{
  InitControls();
  setLayout(main_layout);

  adjustSize();
}

std::optional<USBUtils::DeviceInfo> USBDevicePicker::Run(QWidget* parent, const QString& title,
                                                         FilterFunctionType filter)
{
  USBDevicePicker picker(parent, std::move(filter));
  picker.setWindowTitle(title);

  if (picker.exec() == QDialog::Accepted)
    return picker.GetSelectedDevice();

  return std::nullopt;
}

void USBDevicePicker::InitControls()
{
  m_picker_buttonbox = new QDialogButtonBox();
  auto* select_button = new QPushButton(tr("Select"));
  auto* cancel_button = new QPushButton(tr("Cancel"));
  m_picker_buttonbox->addButton(select_button, QDialogButtonBox::AcceptRole);
  m_picker_buttonbox->addButton(cancel_button, QDialogButtonBox::RejectRole);
  connect(select_button, &QPushButton::clicked, this, &QDialog::accept);
  connect(cancel_button, &QPushButton::clicked, this, &QDialog::reject);
  select_button->setDefault(true);

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
          &USBDevicePicker::OnDeviceSelection);
  connect(usb_inserted_devices_list, &QListWidget::itemDoubleClicked, select_button,
          &QPushButton::clicked);
  connect(m_refresh_devices_timer, &QTimer::timeout, this, &USBDevicePicker::RefreshDeviceList);
  RefreshDeviceList();
  m_refresh_devices_timer->start(1000);

  main_layout->addWidget(usb_inserted_devices_list);
  main_layout->addWidget(m_picker_buttonbox);

  // i18n: VID means Vendor ID (in the context of a USB device)
  device_vid_textbox->setPlaceholderText(tr("Device VID"));
  // i18n: PID means Product ID (in the context of a USB device), not Process ID
  device_pid_textbox->setPlaceholderText(tr("Device PID"));

  const QRegularExpression hex_regex(QStringLiteral("^[0-9A-Fa-f]*$"));
  const QRegularExpressionValidator* hex_validator =
      new QRegularExpressionValidator(hex_regex, this);
  device_vid_textbox->setValidator(hex_validator);
  device_vid_textbox->setMaxLength(4);
  device_pid_textbox->setValidator(hex_validator);
  device_pid_textbox->setMaxLength(4);
}

void USBDevicePicker::RefreshDeviceList()
{
  const auto& current_devices = USBUtils::ListDevices(m_filter);

  if (current_devices == m_shown_devices)
    return;
  const auto selection_string = usb_inserted_devices_list->currentItem();
  usb_inserted_devices_list->clear();
  for (const auto& device : current_devices)
  {
    auto* item = new QListWidgetItem(QString::fromStdString(device.ToDisplayString()),
                                     usb_inserted_devices_list);
    QVariant device_data = QVariant::fromValue(device);
    item->setData(Qt::UserRole, device_data);
  }

  usb_inserted_devices_list->setCurrentItem(selection_string);

  m_shown_devices = current_devices;
}
void USBDevicePicker::OnDeviceSelection()
{
  auto* current_item = usb_inserted_devices_list->currentItem();
  if (!current_item)
    return;

  QVariant item_data = current_item->data(Qt::UserRole);
  USBUtils::DeviceInfo device = item_data.value<USBUtils::DeviceInfo>();

  device_vid_textbox->setText(QString::fromStdString(fmt::format("{:04x}", device.vid)));
  device_pid_textbox->setText(QString::fromStdString(fmt::format("{:04x}", device.pid)));
}

std::optional<USBUtils::DeviceInfo> USBDevicePicker::GetSelectedDevice() const
{
  const std::string vid_string(device_vid_textbox->text().toStdString());
  const std::string pid_string(device_pid_textbox->text().toStdString());

  if (vid_string.empty() || pid_string.empty())
    return std::nullopt;

  const u16 vid = static_cast<u16>(std::stoul(vid_string, nullptr, 16));
  const u16 pid = static_cast<u16>(std::stoul(pid_string, nullptr, 16));

  const USBUtils::DeviceInfo device{vid, pid};

  return device;
}
