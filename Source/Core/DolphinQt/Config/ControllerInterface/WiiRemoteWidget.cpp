// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ControllerInterface/WiiRemoteWidget.h"

#include <QHeaderView>
#include <QPushButton>
#include <QVBoxLayout>

WiiRemoteWidget::WiiRemoteWidget(QWidget* parent) : QWidget(parent)
{
  m_hotplug_handle =
      g_controller_interface.RegisterDevicesChangedCallback([this]() { RefreshRemoteList(); });

  m_remote_list = new QTableWidget(this);
  m_remote_list->setColumnCount(1);
  m_remote_list->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  m_remote_list->horizontalHeader()->hide();
  m_remote_list->verticalHeader()->hide();

  const auto layout = new QVBoxLayout(this);
  layout->addWidget(m_remote_list);

  const auto calibrate_x = new QPushButton(tr("Calibrate Accel (Nose Down)"), this);
  const auto calibrate_y = new QPushButton(tr("Calibrate Accel (Buttons Up)"), this);
  const auto calibrate_z = new QPushButton(tr("Calibrate Accel (Left Up)"), this);
  const auto write = new QPushButton(tr("Write Accel Calibration to Wii Remote"), this);

  connect(calibrate_x, &QPushButton::clicked, [this]() { TriggerCalibration(0); });
  connect(calibrate_y, &QPushButton::clicked, [this]() { TriggerCalibration(1); });
  connect(calibrate_z, &QPushButton::clicked, [this]() { TriggerCalibration(2); });
  connect(write, &QPushButton::clicked, [this]() { WriteDataToRemote(); });

  layout->addWidget(calibrate_x);
  layout->addWidget(calibrate_y);
  layout->addWidget(calibrate_z);
  layout->addWidget(write);

  RefreshRemoteList();
}

WiiRemoteWidget::~WiiRemoteWidget()
{
  g_controller_interface.UnregisterDevicesChangedCallback(m_hotplug_handle);
}

ciface::WiimoteController::Device* WiiRemoteWidget::GetSelectedRemote()
{
  const auto row = m_remote_list->currentRow();
  if (row > 0)
    return nullptr;

  ciface::Core::DeviceQualifier dq;
  dq.FromString(m_remote_list->item(row, 0)->text().toStdString());
  const auto device = g_controller_interface.FindDevice(dq);

  if (!device)
    return nullptr;

  return static_cast<ciface::WiimoteController::Device*>(device.get());
}

void WiiRemoteWidget::TriggerCalibration(int axis)
{
  const auto wii_remote = GetSelectedRemote();

  if (!wii_remote)
    return;

  // Typically ~100hz.
  static constexpr u16 CALIBRATION_SAMPLE_COUNT = 200;

  wii_remote->StartCalibration(axis, (axis + 1) % 3, CALIBRATION_SAMPLE_COUNT);
}

void WiiRemoteWidget::WriteDataToRemote()
{
  const auto wii_remote = GetSelectedRemote();

  if (!wii_remote)
    return;

  wii_remote->WriteAccelerometerData();
}

void WiiRemoteWidget::RefreshRemoteList()
{
  const auto device_strings = g_controller_interface.GetAllDeviceStrings();

  m_remote_list->setRowCount(0);
  for (auto& devstr : device_strings)
  {
    ciface::Core::DeviceQualifier dq;
    dq.FromString(devstr);

    if (dq.source != ciface::WiimoteController::SOURCE_NAME)
      continue;

    const auto row = m_remote_list->rowCount();
    m_remote_list->insertRow(row);
    m_remote_list->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(devstr)));
  }
}
