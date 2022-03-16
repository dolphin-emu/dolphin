// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QTableWidget>
#include <QWidget>

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Wiimote/WiimoteController.h"

class WiiRemoteWidget final : public QWidget
{
public:
  WiiRemoteWidget(QWidget* parent);
  ~WiiRemoteWidget();

private:
  void RefreshRemoteList();
  void TriggerCalibration(int axis);
  void WriteDataToRemote();

  ciface::WiimoteController::Device* GetSelectedRemote();

  QTableWidget* m_remote_list;
  ControllerInterface::HotplugCallbackHandle m_hotplug_handle;
};
