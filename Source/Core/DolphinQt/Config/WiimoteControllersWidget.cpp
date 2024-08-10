// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/WiimoteControllersWidget.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QScreen>
#include <QVBoxLayout>

#include <optional>

#include "Common/Config/Config.h"

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

#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

WiimoteControllersWidget::WiimoteControllersWidget(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  ConnectWidgets();

  connect(&Settings::Instance(), &Settings::ConfigChanged, this,
          [this] { LoadSettings(Core::GetState(Core::System::GetInstance())); });
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          [this](Core::State state) { LoadSettings(state); });
  LoadSettings(Core::GetState(Core::System::GetInstance()));
}

void WiimoteControllersWidget::UpdateBluetoothAvailableStatus()
{
  m_bluetooth_unavailable->setHidden(WiimoteReal::IsScannerReady());
}

static int GetRadioButtonIndicatorWidth()
{
  const QStyle* style = QApplication::style();
  QStyleOptionButton opt;

  // TODO: why does the macOS style act different? Is it because of the magic with
  // Cocoa widgets it does behind the scenes?
  if (style->objectName() == QStringLiteral("macintosh"))
    return style->subElementRect(QStyle::SE_RadioButtonIndicator, &opt).width();

  return style->subElementRect(QStyle::SE_RadioButtonContents, &opt).left();
}

static int GetLayoutHorizontalSpacing(const QGridLayout* layout)
{
  // TODO: shouldn't layout->horizontalSpacing() do all this? Why does it return -1?
  int hspacing = layout->horizontalSpacing();
  if (hspacing >= 0)
    return hspacing;

  // According to docs, this is the fallback if horizontalSpacing() isn't set.
  auto style = layout->parentWidget()->style();
  hspacing = style->pixelMetric(QStyle::PM_LayoutHorizontalSpacing);
  if (hspacing >= 0)
    return hspacing;

  // Docs claim this is deprecated, but on macOS with Qt 5.8 this is the only one that actually
  // works.
  float pixel_ratio = QGuiApplication::primaryScreen()->devicePixelRatio();
#ifdef __APPLE__
  // TODO is this still required?
  hspacing = pixel_ratio * style->pixelMetric(QStyle::PM_LayoutHorizontalSpacing);
  if (hspacing >= 0)
    return hspacing;
#endif

  // Ripped from qtbase/src/widgets/styles/qcommonstyle.cpp
  return pixel_ratio * 6;
}

void WiimoteControllersWidget::CreateLayout()
{
  m_wiimote_layout = new QGridLayout();
  m_wiimote_box = new QGroupBox(tr("Wii Remotes"));
  m_wiimote_box->setLayout(m_wiimote_layout);

  m_wiimote_passthrough = new QRadioButton(tr("Passthrough a Bluetooth adapter"));
  m_wiimote_sync = new NonDefaultQPushButton(tr("Sync"));
  m_wiimote_reset = new NonDefaultQPushButton(tr("Reset"));
  m_wiimote_refresh = new NonDefaultQPushButton(tr("Refresh"));
  m_wiimote_pt_labels[0] = new QLabel(tr("Sync real Wii Remotes and pair them"));
  m_wiimote_pt_labels[1] = new QLabel(tr("Reset all saved Wii Remote pairings"));
  m_wiimote_emu = new QRadioButton(tr("Emulate the Wii's Bluetooth adapter"));
  m_wiimote_continuous_scanning = new QCheckBox(tr("Continuous Scanning"));
  m_wiimote_real_balance_board = new QCheckBox(tr("Real Balance Board"));
  m_wiimote_speaker_data = new QCheckBox(tr("Enable Speaker Data"));
  m_wiimote_ciface = new QCheckBox(tr("Connect Wii Remotes for Emulated Controllers"));

  m_wiimote_layout->setVerticalSpacing(7);
  m_wiimote_layout->setColumnMinimumWidth(0, GetRadioButtonIndicatorWidth() -
                                                 GetLayoutHorizontalSpacing(m_wiimote_layout));
  m_wiimote_layout->setColumnStretch(2, 1);

  // Passthrough BT
  m_wiimote_layout->addWidget(m_wiimote_passthrough, m_wiimote_layout->rowCount(), 0, 1, -1);

  int sync_row = m_wiimote_layout->rowCount();
  m_wiimote_layout->addWidget(m_wiimote_pt_labels[0], sync_row, 1, 1, 2);
  m_wiimote_layout->addWidget(m_wiimote_sync, sync_row, 3);

  int reset_row = m_wiimote_layout->rowCount();
  m_wiimote_layout->addWidget(m_wiimote_pt_labels[1], reset_row, 1, 1, 2);
  m_wiimote_layout->addWidget(m_wiimote_reset, reset_row, 3);

  // Emulated BT
  m_wiimote_layout->addWidget(m_wiimote_emu, m_wiimote_layout->rowCount(), 0, 1, -1);

  for (size_t i = 0; i < m_wiimote_groups.size(); i++)
  {
    auto* wm_label = m_wiimote_labels[i] = new QLabel(tr("Wii Remote %1").arg(i + 1));
    auto* wm_box = m_wiimote_boxes[i] = new QComboBox();
    auto* wm_button = m_wiimote_buttons[i] = new NonDefaultQPushButton(tr("Configure"));

    for (const auto& item : {tr("None"), tr("Emulated Wii Remote"), tr("Real Wii Remote")})
      wm_box->addItem(item);

    int wm_row = m_wiimote_layout->rowCount();
    m_wiimote_layout->addWidget(wm_label, wm_row, 1);
    m_wiimote_layout->addWidget(wm_box, wm_row, 2);
    m_wiimote_layout->addWidget(wm_button, wm_row, 3);
  }

  m_wiimote_layout->addWidget(m_wiimote_real_balance_board, m_wiimote_layout->rowCount(), 1, 1, -1);
  m_wiimote_layout->addWidget(m_wiimote_speaker_data, m_wiimote_layout->rowCount(), 1, 1, -1);

  m_wiimote_layout->addWidget(m_wiimote_ciface, m_wiimote_layout->rowCount(), 0, 1, -1);

  int continuous_scanning_row = m_wiimote_layout->rowCount();
  m_wiimote_layout->addWidget(m_wiimote_continuous_scanning, continuous_scanning_row, 0, 1, 3);
  m_wiimote_layout->addWidget(m_wiimote_refresh, continuous_scanning_row, 3);

  m_bluetooth_unavailable = new QLabel(tr("A supported Bluetooth device could not be found.\n"
                                          "You must manually connect your Wii Remote."));
  m_wiimote_layout->addWidget(m_bluetooth_unavailable, m_wiimote_layout->rowCount(), 1, 1, -1);

  auto* layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setAlignment(Qt::AlignTop);
  layout->addWidget(m_wiimote_box);
  setLayout(layout);
}

void WiimoteControllersWidget::ConnectWidgets()
{
  connect(m_wiimote_passthrough, &QRadioButton::toggled, this, [this] {
    SaveSettings();
    LoadSettings(Core::GetState(Core::System::GetInstance()));
  });
  connect(m_wiimote_ciface, &QCheckBox::toggled, this, [this] {
    SaveSettings();
    LoadSettings(Core::GetState(Core::System::GetInstance()));
    WiimoteReal::HandleWiimotesInControllerInterfaceSettingChange();
  });
  connect(m_wiimote_continuous_scanning, &QCheckBox::toggled, this, [this] {
    SaveSettings();
    LoadSettings(Core::GetState(Core::System::GetInstance()));
  });

  connect(m_wiimote_real_balance_board, &QCheckBox::toggled, this,
          &WiimoteControllersWidget::SaveSettings);
  connect(m_wiimote_speaker_data, &QCheckBox::toggled, this,
          &WiimoteControllersWidget::SaveSettings);
  connect(m_wiimote_sync, &QPushButton::clicked, this,
          &WiimoteControllersWidget::OnBluetoothPassthroughSyncPressed);
  connect(m_wiimote_reset, &QPushButton::clicked, this,
          &WiimoteControllersWidget::OnBluetoothPassthroughResetPressed);
  connect(m_wiimote_refresh, &QPushButton::clicked, this,
          &WiimoteControllersWidget::OnWiimoteRefreshPressed);

  for (size_t i = 0; i < m_wiimote_groups.size(); i++)
  {
    connect(m_wiimote_boxes[i], &QComboBox::currentIndexChanged, this, [this] {
      SaveSettings();
      LoadSettings(Core::GetState(Core::System::GetInstance()));
    });
    connect(m_wiimote_buttons[i], &QPushButton::clicked, this,
            [this, i] { OnWiimoteConfigure(i); });
  }
}

void WiimoteControllersWidget::OnBluetoothPassthroughResetPressed()
{
  const auto ios = Core::System::GetInstance().GetIOS();

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
  const auto ios = Core::System::GetInstance().GetIOS();

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
  MappingWindow::Type type;
  switch (m_wiimote_boxes[index]->currentIndex())
  {
  case 0:  // None
  case 2:  // Real Wii Remote
    return;
  case 1:  // Emulated Wii Remote
    type = MappingWindow::Type::MAPPING_WIIMOTE_EMU;
    break;
  default:
    return;
  }

  MappingWindow* window = new MappingWindow(this, type, static_cast<int>(index));
  window->setAttribute(Qt::WA_DeleteOnClose, true);
  window->setWindowModality(Qt::WindowModality::WindowModal);
  SetQWidgetWindowDecorations(window);
  window->show();
}

void WiimoteControllersWidget::LoadSettings(Core::State state)
{
  for (size_t i = 0; i < m_wiimote_groups.size(); i++)
  {
    SignalBlocking(m_wiimote_boxes[i])
        ->setCurrentIndex(int(Config::Get(Config::GetInfoForWiimoteSource(int(i)))));
  }
  SignalBlocking(m_wiimote_real_balance_board)
      ->setChecked(Config::Get(Config::WIIMOTE_BB_SOURCE) == WiimoteSource::Real);
  SignalBlocking(m_wiimote_speaker_data)
      ->setChecked(Config::Get(Config::MAIN_WIIMOTE_ENABLE_SPEAKER));
  SignalBlocking(m_wiimote_ciface)
      ->setChecked(Config::Get(Config::MAIN_CONNECT_WIIMOTES_FOR_CONTROLLER_INTERFACE));
  SignalBlocking(m_wiimote_continuous_scanning)
      ->setChecked(Config::Get(Config::MAIN_WIIMOTE_CONTINUOUS_SCANNING));

  if (Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_ENABLED))
    SignalBlocking(m_wiimote_passthrough)->setChecked(true);
  else
    SignalBlocking(m_wiimote_emu)->setChecked(true);

  // Make sure continuous scanning setting is applied.
  WiimoteReal::Initialize(::Wiimote::InitializeMode::DO_NOT_WAIT_FOR_WIIMOTES);

  const bool running = state != Core::State::Uninitialized;

  m_wiimote_emu->setEnabled(!running);
  m_wiimote_passthrough->setEnabled(!running);

  const bool running_gc = running && !Core::System::GetInstance().IsWii();
  const bool enable_passthrough = m_wiimote_passthrough->isChecked() && !running_gc;
  const bool enable_emu_bt = !m_wiimote_passthrough->isChecked() && !running_gc;
  const bool is_netplay = NetPlay::IsNetPlayRunning();
  const bool running_netplay = running && is_netplay;

  m_wiimote_sync->setEnabled(enable_passthrough);
  m_wiimote_reset->setEnabled(enable_passthrough);

  for (auto* pt_label : m_wiimote_pt_labels)
    pt_label->setEnabled(enable_passthrough);

  const int num_local_wiimotes = is_netplay ? NetPlay::NumLocalWiimotes() : 4;
  for (size_t i = 0; i < m_wiimote_groups.size(); i++)
  {
    m_wiimote_labels[i]->setEnabled(enable_emu_bt);
    m_wiimote_boxes[i]->setEnabled(enable_emu_bt && !running_netplay);

    const bool is_emu_wiimote = m_wiimote_boxes[i]->currentIndex() == 1;
    m_wiimote_buttons[i]->setEnabled(enable_emu_bt && is_emu_wiimote &&
                                     static_cast<int>(i) < num_local_wiimotes);
  }

  m_wiimote_real_balance_board->setEnabled(enable_emu_bt && !running_netplay);
  m_wiimote_speaker_data->setEnabled(enable_emu_bt && !running_netplay);

  const bool ciface_wiimotes = m_wiimote_ciface->isChecked();

  m_wiimote_refresh->setEnabled((enable_emu_bt || ciface_wiimotes) &&
                                !m_wiimote_continuous_scanning->isChecked());
  m_wiimote_continuous_scanning->setEnabled(enable_emu_bt || ciface_wiimotes);
}

void WiimoteControllersWidget::SaveSettings()
{
  {
    Config::ConfigChangeCallbackGuard config_guard;
    Config::SetBaseOrCurrent(Config::MAIN_WIIMOTE_ENABLE_SPEAKER,
                             m_wiimote_speaker_data->isChecked());
    Config::SetBaseOrCurrent(Config::MAIN_CONNECT_WIIMOTES_FOR_CONTROLLER_INTERFACE,
                             m_wiimote_ciface->isChecked());
    Config::SetBaseOrCurrent(Config::MAIN_WIIMOTE_CONTINUOUS_SCANNING,
                             m_wiimote_continuous_scanning->isChecked());
    Config::SetBaseOrCurrent(Config::MAIN_BLUETOOTH_PASSTHROUGH_ENABLED,
                             m_wiimote_passthrough->isChecked());

    const WiimoteSource bb_source =
        m_wiimote_real_balance_board->isChecked() ? WiimoteSource::Real : WiimoteSource::None;
    Config::SetBaseOrCurrent(Config::WIIMOTE_BB_SOURCE, bb_source);

    for (size_t i = 0; i < m_wiimote_groups.size(); i++)
    {
      const int index = m_wiimote_boxes[i]->currentIndex();
      Config::SetBaseOrCurrent(Config::GetInfoForWiimoteSource(int(i)), WiimoteSource(index));
    }
  }

  SConfig::GetInstance().SaveSettings();
}
