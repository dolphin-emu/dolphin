// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include <QGroupBox>

#include "Common/WorkQueueThread.h"
#include "Core/HW/Wiimote.h"
#include "Core/IOS/USB/Bluetooth/BTReal.h"

class QWidget;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;

namespace Core
{
enum class State;
}

class WiimoteControllersWidget final : public QGroupBox
{
  Q_OBJECT
public:
  explicit WiimoteControllersWidget(QWidget* parent);
  ~WiimoteControllersWidget() override;

  void UpdateBluetoothAvailableStatus();

private:
  void SaveSettings();
  void OnBluetoothPassthroughDeviceChanged(int index);
  void OnBluetoothPassthroughSyncPressed();
  void OnBluetoothPassthroughResetPressed();
  void OnBluetoothAdapterRefreshComplete(
      const std::vector<IOS::HLE::BluetoothRealDevice::BluetoothDeviceInfo>& devices);
  static void OnWiimoteRefreshPressed();
  void OnWiimoteConfigure(size_t index);
  void StartBluetoothAdapterRefresh();
  void UpdateEnabledWidgets(Core::State state);

  void CreateLayout();
  void ConnectWidgets();
  void LoadSettings(Core::State state);

  // Widgets that should be disabled when running GameCube
  QWidget* m_wii_contents;

  QWidget* m_emulated_bt_contents;
  QWidget* m_passthru_bt_contents;

  std::array<QComboBox*, MAX_WIIMOTES> m_wiimote_boxes;
  std::array<QPushButton*, MAX_WIIMOTES> m_wiimote_buttons;

  Common::AsyncWorkThreadSP m_bluetooth_adapter_refresh_thread;
  bool m_bluetooth_adapter_scan_in_progress = false;

  QRadioButton* m_wiimote_emu;
  QRadioButton* m_wiimote_passthrough;

  QLabel* m_bluetooth_adapters_label;
  QComboBox* m_bluetooth_adapters;
  QPushButton* m_bluetooth_adapters_refresh;

  QPushButton* m_wiimote_sync;
  QPushButton* m_wiimote_reset;

  QCheckBox* m_wiimote_continuous_scanning;
  QCheckBox* m_wiimote_real_balance_board;
  QCheckBox* m_wiimote_speaker_data;
  QCheckBox* m_wiimote_ciface;
  QPushButton* m_wiimote_refresh;
  QLabel* m_bluetooth_unavailable;
};
