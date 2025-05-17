// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/WiimoteControllersWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QVariant>

#include "Common/Config/Config.h"
#include "Common/WorkQueueThread.h"

#include "Core/Config/MainSettings.h"
#include "Core/Config/WiimoteSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Bluetooth/BTReal.h"
#include "Core/NetPlayProto.h"
#include "Core/System.h"
#include "Core/WiiUtils.h"

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

WiimoteControllersWidget::WiimoteControllersWidget(QWidget* parent)
    : QGroupBox{tr("Wii Remotes"), parent}
{
  CreateLayout();
  ConnectWidgets();

  connect(&Settings::Instance(), &Settings::ConfigChanged, this,
          [this] { LoadSettings(Core::GetState(Core::System::GetInstance())); });

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          [this](Core::State state) { UpdateEnabledWidgets(state); });

  LoadSettings(Core::GetState(Core::System::GetInstance()));

  m_bluetooth_adapter_refresh_thread.Reset("Bluetooth Adapter Refresh Thread");
  StartBluetoothAdapterRefresh();
}

WiimoteControllersWidget::~WiimoteControllersWidget()
{
  m_bluetooth_adapter_refresh_thread.WaitForCompletion();
}

void WiimoteControllersWidget::UpdateBluetoothAvailableStatus()
{
  m_bluetooth_unavailable->setHidden(WiimoteReal::IsScannerReady());
}

void WiimoteControllersWidget::StartBluetoothAdapterRefresh()
{
  if (m_bluetooth_adapter_scan_in_progress)
    return;

  m_bluetooth_adapters->clear();
  m_bluetooth_adapters->setDisabled(true);
  m_bluetooth_adapters->addItem(tr("Scanning for adapters..."));

  m_bluetooth_adapter_scan_in_progress = true;

  const auto scan_func = [this]() {
    INFO_LOG_FMT(COMMON, "Refreshing Bluetooth adapter list...");
    auto device_list = IOS::HLE::BluetoothRealDevice::ListDevices();
    INFO_LOG_FMT(COMMON, "{} Bluetooth adapters available.", device_list.size());
    const auto refresh_complete_func = [this, devices = std::move(device_list)]() {
      OnBluetoothAdapterRefreshComplete(devices);
    };
    QueueOnObject(this, refresh_complete_func);
  };

  m_bluetooth_adapter_refresh_thread.Push(scan_func);
}

void WiimoteControllersWidget::OnBluetoothAdapterRefreshComplete(
    const std::vector<IOS::HLE::BluetoothRealDevice::BluetoothDeviceInfo>& devices)
{
  const int configured_vid = Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_VID);
  const int configured_pid = Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_PID);
  bool found_configured_device = configured_vid == -1 || configured_pid == -1;

  m_bluetooth_adapters->clear();
  m_bluetooth_adapter_scan_in_progress = false;

  const auto state = Core::GetState(Core::System::GetInstance());
  UpdateEnabledWidgets(state);

  m_bluetooth_adapters->addItem(tr("Automatic"));

  for (const auto& device : devices)
  {
    std::string name = device.name.empty() ? tr("Unknown Device").toStdString() : device.name;
    QString device_info =
        QString::fromStdString(fmt::format("{} ({:04x}:{:04x})", name, device.vid, device.pid));
    m_bluetooth_adapters->addItem(device_info, QVariant::fromValue(device));

    if (!found_configured_device &&
        IOS::HLE::BluetoothRealDevice::IsConfiguredBluetoothDevice(device.vid, device.pid))
    {
      found_configured_device = true;
      m_bluetooth_adapters->setCurrentIndex(m_bluetooth_adapters->count() - 1);
    }
  }

  if (!found_configured_device)
  {
    const QString name = QLatin1Char{'['} + tr("disconnected") + QLatin1Char(']');
    const std::string name_str = name.toStdString();

    IOS::HLE::BluetoothRealDevice::BluetoothDeviceInfo disconnected_device;
    disconnected_device.vid = configured_vid;
    disconnected_device.pid = configured_pid;
    disconnected_device.name = name_str;

    QString device_info = QString::fromStdString(
        fmt::format("{} ({:04x}:{:04x})", name_str, configured_vid, configured_pid));

    m_bluetooth_adapters->insertSeparator(m_bluetooth_adapters->count());
    m_bluetooth_adapters->addItem(device_info, QVariant::fromValue(disconnected_device));
    m_bluetooth_adapters->setCurrentIndex(m_bluetooth_adapters->count() - 1);
  }
}

void WiimoteControllersWidget::CreateLayout()
{
  auto* const layout = new QVBoxLayout{this};

  m_wiimote_passthrough = new QRadioButton(tr("Passthrough a Bluetooth adapter"));
  m_wiimote_emu = new QRadioButton(tr("Emulate the Wii's Bluetooth adapter"));

  m_bluetooth_adapters_label = new QLabel(tr("Adapter"));
  m_bluetooth_adapters = new QComboBox;
  m_bluetooth_adapters_refresh = new NonDefaultQPushButton(tr("Refresh"));

  m_wiimote_sync = new NonDefaultQPushButton(tr("Sync"));
  m_wiimote_reset = new NonDefaultQPushButton(tr("Reset"));

  m_wiimote_refresh = new NonDefaultQPushButton(tr("Refresh"));

  auto* const sync_label = new QLabel(tr("Sync real Wii Remotes and pair them"));
  auto* const reset_label = new QLabel(tr("Reset all saved Wii Remote pairings"));

  m_wiimote_continuous_scanning =
      new ConfigBool(tr("Continuous Scanning"), Config::MAIN_WIIMOTE_CONTINUOUS_SCANNING);
  m_wiimote_real_balance_board = new QCheckBox(tr("Real Balance Board"));
  m_wiimote_speaker_data =
      new ConfigBool(tr("Enable Speaker Data"), Config::MAIN_WIIMOTE_ENABLE_SPEAKER);
  m_wiimote_ciface = new ConfigBool(tr("Connect Wii Remotes for Emulated Controllers"),
                                    Config::MAIN_CONNECT_WIIMOTES_FOR_CONTROLLER_INTERFACE);

  m_wii_contents = new QWidget;
  layout->addWidget(m_wii_contents);
  auto* const wii_layout = new QVBoxLayout{m_wii_contents};
  const int under_radio_left_margin = wii_layout->contentsMargins().left();
  wii_layout->setContentsMargins(QMargins{});

  wii_layout->addWidget(m_wiimote_passthrough);
  wii_layout->addWidget(m_wiimote_emu);

  // Passthrough BT
  m_passthru_bt_contents = new QWidget;
  wii_layout->addWidget(m_passthru_bt_contents);
  auto* const passthru_bt_layout = new QGridLayout{m_passthru_bt_contents};
  passthru_bt_layout->setContentsMargins(QMargins{under_radio_left_margin, 0, 0, 0});
  passthru_bt_layout->setColumnStretch(1, 1);

  const int adapter_row = passthru_bt_layout->rowCount();
  passthru_bt_layout->addWidget(m_bluetooth_adapters_label, adapter_row, 0, 1, 1);
  passthru_bt_layout->addWidget(m_bluetooth_adapters, adapter_row, 1, 1, 1);
  passthru_bt_layout->addWidget(m_bluetooth_adapters_refresh, adapter_row, 2, 1, 1);

  const int sync_row = passthru_bt_layout->rowCount();
  passthru_bt_layout->addWidget(sync_label, sync_row, 0, 1, 2);
  passthru_bt_layout->addWidget(m_wiimote_sync, sync_row, 2);

  const int reset_row = passthru_bt_layout->rowCount();
  passthru_bt_layout->addWidget(reset_label, reset_row, 0, 1, 2);
  passthru_bt_layout->addWidget(m_wiimote_reset, reset_row, 2);

  // Emulated BT
  m_emulated_bt_contents = new QWidget;
  wii_layout->addWidget(m_emulated_bt_contents);
  auto* const emulated_bt_layout = new QGridLayout{m_emulated_bt_contents};
  emulated_bt_layout->setContentsMargins(QMargins{under_radio_left_margin, 0, 0, 0});
  emulated_bt_layout->setColumnStretch(1, 1);

  for (size_t i = 0; i != m_wiimote_boxes.size(); ++i)
  {
    auto* const wm_label = new QLabel(tr("Wii Remote %1").arg(i + 1));
    auto* const wm_box = m_wiimote_boxes[i] = new QComboBox;
    auto* const wm_button = m_wiimote_buttons[i] = new NonDefaultQPushButton(tr("Configure"));

    for (const auto& item : {tr("None"), tr("Emulated Wii Remote"), tr("Real Wii Remote")})
      wm_box->addItem(item);

    const int wm_row = emulated_bt_layout->rowCount();
    emulated_bt_layout->addWidget(wm_label, wm_row, 0);
    emulated_bt_layout->addWidget(wm_box, wm_row, 1);
    emulated_bt_layout->addWidget(wm_button, wm_row, 2);
  }

  emulated_bt_layout->addWidget(m_wiimote_real_balance_board, emulated_bt_layout->rowCount(), 0, 1,
                                -1);
  emulated_bt_layout->addWidget(m_wiimote_speaker_data, emulated_bt_layout->rowCount(), 0, 1, -1);

  auto* const scanning_layout = new QGridLayout;
  scanning_layout->setColumnStretch(0, 1);
  layout->addLayout(scanning_layout);

  scanning_layout->addWidget(m_wiimote_ciface, 0, 0);
  scanning_layout->addWidget(m_wiimote_continuous_scanning, 1, 0);
  scanning_layout->addWidget(m_wiimote_refresh, 0, 1, 2, 1, Qt::AlignBottom);

  m_bluetooth_unavailable = new QLabel(tr("A supported Bluetooth device could not be found.\n"
                                          "You must manually connect your Wii Remote."));
  layout->addWidget(m_bluetooth_unavailable);
}

void WiimoteControllersWidget::ConnectWidgets()
{
  connect(m_wiimote_passthrough, &QRadioButton::toggled, this, [this] { SaveSettings(); });

  connect(m_wiimote_ciface, &QCheckBox::toggled, this,
          [] { WiimoteReal::HandleWiimotesInControllerInterfaceSettingChange(); });

  connect(m_wiimote_real_balance_board, &QCheckBox::toggled, this,
          &WiimoteControllersWidget::SaveSettings);

  connect(m_bluetooth_adapters, &QComboBox::activated, this,
          &WiimoteControllersWidget::OnBluetoothPassthroughDeviceChanged);
  connect(m_bluetooth_adapters_refresh, &QPushButton::clicked, this,
          &WiimoteControllersWidget::StartBluetoothAdapterRefresh);

  connect(m_wiimote_sync, &QPushButton::clicked, this,
          &WiimoteControllersWidget::OnBluetoothPassthroughSyncPressed);
  connect(m_wiimote_reset, &QPushButton::clicked, this,
          &WiimoteControllersWidget::OnBluetoothPassthroughResetPressed);

  connect(m_wiimote_refresh, &QPushButton::clicked, this,
          &WiimoteControllersWidget::OnWiimoteRefreshPressed);

  for (size_t i = 0; i != m_wiimote_boxes.size(); ++i)
  {
    connect(m_wiimote_boxes[i], &QComboBox::currentIndexChanged, this, [this] { SaveSettings(); });
    connect(m_wiimote_buttons[i], &QPushButton::clicked, this,
            [this, i] { OnWiimoteConfigure(i); });
  }
}

void WiimoteControllersWidget::OnBluetoothPassthroughDeviceChanged(int index)
{
  // "Automatic" selection
  if (index == 0)
  {
    Config::DeleteKey(Config::GetActiveLayerForConfig(Config::MAIN_BLUETOOTH_PASSTHROUGH_PID),
                      Config::MAIN_BLUETOOTH_PASSTHROUGH_PID);
    Config::DeleteKey(Config::GetActiveLayerForConfig(Config::MAIN_BLUETOOTH_PASSTHROUGH_VID),
                      Config::MAIN_BLUETOOTH_PASSTHROUGH_VID);
    return;
  }

  auto device_info = m_bluetooth_adapters->itemData(index)
                         .value<IOS::HLE::BluetoothRealDevice::BluetoothDeviceInfo>();

  Config::SetBaseOrCurrent(Config::MAIN_BLUETOOTH_PASSTHROUGH_PID, device_info.pid);
  Config::SetBaseOrCurrent(Config::MAIN_BLUETOOTH_PASSTHROUGH_VID, device_info.vid);
}

void WiimoteControllersWidget::OnBluetoothPassthroughResetPressed()
{
  auto* const ios = Core::System::GetInstance().GetIOS();

  if (!ios)
  {
    ModalMessageBox::warning(
        this, tr("Warning"),
        tr("Saved Wii Remote pairings can only be reset when a Wii game is running."));
    return;
  }

  auto device = WiiUtils::GetBluetoothRealDevice();
  if (device != nullptr)
    device->TriggerSyncButtonHeldEvent();
}

void WiimoteControllersWidget::OnBluetoothPassthroughSyncPressed()
{
  auto* const ios = Core::System::GetInstance().GetIOS();

  if (!ios)
  {
    ModalMessageBox::warning(this, tr("Warning"),
                             tr("A sync can only be triggered when a Wii game is running."));
    return;
  }

  auto device = WiiUtils::GetBluetoothRealDevice();
  if (device != nullptr)
    device->TriggerSyncButtonPressedEvent();
}

void WiimoteControllersWidget::OnWiimoteRefreshPressed()
{
  WiimoteReal::Refresh();
}

void WiimoteControllersWidget::OnWiimoteConfigure(size_t index)
{
  const auto type = MappingWindow::Type::MAPPING_WIIMOTE_EMU;
  auto* const window = new MappingWindow(this, type, static_cast<int>(index));
  window->setAttribute(Qt::WA_DeleteOnClose, true);
  window->setWindowModality(Qt::WindowModality::WindowModal);
  SetQWidgetWindowDecorations(window);
  window->show();
}

void WiimoteControllersWidget::UpdateEnabledWidgets(const Core::State state)
{
  const bool is_running = state != Core::State::Uninitialized;
  const bool is_running_gc = is_running && !Core::System::GetInstance().IsWii();

  const bool enable_adapter_selection = !is_running && !m_bluetooth_adapter_scan_in_progress;
  const bool enable_adapter_refresh = !is_running;

  m_bluetooth_adapters_label->setEnabled(enable_adapter_selection);
  m_bluetooth_adapters->setEnabled(enable_adapter_selection);
  m_bluetooth_adapters_refresh->setEnabled(enable_adapter_refresh);

  const bool is_passthru_bt = m_wiimote_passthrough->isChecked();

  // These are not relevant in GC mode.
  m_wii_contents->setDisabled(is_running_gc);

  // Disable the radio buttons when running.
  m_wiimote_emu->setEnabled(!is_running);
  m_wiimote_passthrough->setEnabled(!is_running);

  // Temporarily hide both to prevent an ugly flash with both visible.
  m_passthru_bt_contents->setVisible(false);
  m_emulated_bt_contents->setVisible(false);

  m_passthru_bt_contents->setVisible(is_passthru_bt);
  m_emulated_bt_contents->setVisible(!is_passthru_bt);

  const bool is_netplay_enabled = NetPlay::IsNetPlayRunning();
  const bool is_running_netplay = is_running && is_netplay_enabled;

  // Disable non-local wii remotes during NetPlay.
  const auto num_local_wiimotes = size_t(is_netplay_enabled ? NetPlay::NumLocalWiimotes() : 4);
  int num_real_wiimotes = 0;
  for (size_t i = 0; i != m_wiimote_boxes.size(); ++i)
  {
    num_real_wiimotes += m_wiimote_boxes[i]->currentIndex() == 2;

    m_wiimote_boxes[i]->setEnabled(!is_running_netplay);

    const bool is_emu_wiimote = m_wiimote_boxes[i]->currentIndex() == 1;
    m_wiimote_buttons[i]->setEnabled(is_emu_wiimote && (i < num_local_wiimotes));
  }

  num_real_wiimotes += m_wiimote_real_balance_board->isChecked();

  m_wiimote_real_balance_board->setEnabled(!is_running_netplay);
  m_wiimote_speaker_data->setEnabled(!is_running_netplay);

  const bool using_emu_bt = m_wiimote_emu->isChecked() && !is_running_gc;
  const bool ciface_wiimotes = m_wiimote_ciface->isChecked();

  m_wiimote_continuous_scanning->setEnabled(using_emu_bt || ciface_wiimotes);
  m_wiimote_refresh->setEnabled(((using_emu_bt && num_real_wiimotes != 0) || ciface_wiimotes) &&
                                !m_wiimote_continuous_scanning->isChecked());
}

void WiimoteControllersWidget::LoadSettings(Core::State state)
{
  for (size_t i = 0; i != m_wiimote_boxes.size(); ++i)
  {
    SignalBlocking(m_wiimote_boxes[i])
        ->setCurrentIndex(int(Config::Get(Config::GetInfoForWiimoteSource(int(i)))));
  }
  SignalBlocking(m_wiimote_real_balance_board)
      ->setChecked(Config::Get(Config::WIIMOTE_BB_SOURCE) == WiimoteSource::Real);

  SignalBlocking(Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_ENABLED) ? m_wiimote_passthrough :
                                                                           m_wiimote_emu)
      ->setChecked(true);

  // Make sure continuous scanning setting is applied.
  WiimoteReal::Initialize(::Wiimote::InitializeMode::DO_NOT_WAIT_FOR_WIIMOTES);

  UpdateEnabledWidgets(state);
}

void WiimoteControllersWidget::SaveSettings()
{
  {
    Config::ConfigChangeCallbackGuard config_guard;

    Config::SetBaseOrCurrent(Config::MAIN_BLUETOOTH_PASSTHROUGH_ENABLED,
                             m_wiimote_passthrough->isChecked());

    const WiimoteSource bb_source =
        m_wiimote_real_balance_board->isChecked() ? WiimoteSource::Real : WiimoteSource::None;
    Config::SetBaseOrCurrent(Config::WIIMOTE_BB_SOURCE, bb_source);

    for (size_t i = 0; i != m_wiimote_boxes.size(); ++i)
    {
      const int index = m_wiimote_boxes[i]->currentIndex();
      Config::SetBaseOrCurrent(Config::GetInfoForWiimoteSource(int(i)), WiimoteSource(index));
    }
  }

  SConfig::GetInstance().SaveSettings();
}
