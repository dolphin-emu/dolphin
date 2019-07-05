// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/ControllersWindow.h"

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QVBoxLayout>

#include <map>
#include <optional>

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Bluetooth/BTReal.h"

#include "DolphinQt/Config/Mapping/GCPadWiiUConfigDialog.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"
#include "DolphinQt/Settings.h"

#include "InputCommon/GCAdapter.h"

#include "UICommon/UICommon.h"

static const std::map<SerialInterface::SIDevices, int> s_gc_types = {
    {SerialInterface::SIDEVICE_NONE, 0},         {SerialInterface::SIDEVICE_GC_CONTROLLER, 1},
    {SerialInterface::SIDEVICE_WIIU_ADAPTER, 2}, {SerialInterface::SIDEVICE_GC_STEERING, 3},
    {SerialInterface::SIDEVICE_DANCEMAT, 4},     {SerialInterface::SIDEVICE_GC_TARUKONGA, 5},
    {SerialInterface::SIDEVICE_GC_GBA, 6},       {SerialInterface::SIDEVICE_GC_KEYBOARD, 7}};

static std::optional<int> ToGCMenuIndex(const SerialInterface::SIDevices sidevice)
{
  auto it = s_gc_types.find(sidevice);
  return it != s_gc_types.end() ? it->second : std::optional<int>();
}

static std::optional<SerialInterface::SIDevices> FromGCMenuIndex(const int menudevice)
{
  auto it = std::find_if(s_gc_types.begin(), s_gc_types.end(),
                         [=](auto pair) { return pair.second == menudevice; });
  return it != s_gc_types.end() ? it->first : std::optional<SerialInterface::SIDevices>();
}

ControllersWindow::ControllersWindow(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Controller Settings"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  CreateGamecubeLayout();
  CreateWiimoteLayout();
  CreateAdvancedLayout();
  CreateMainLayout();
  LoadSettings();
  ConnectWidgets();

  OnEmulationStateChanged(Core::GetState() != Core::State::Uninitialized);
}

void ControllersWindow::CreateGamecubeLayout()
{
  m_gc_box = new QGroupBox(tr("GameCube Controllers"));
  m_gc_layout = new QGridLayout();
  m_gc_layout->setVerticalSpacing(7);
  m_gc_layout->setColumnStretch(1, 1);

  for (size_t i = 0; i < m_gc_groups.size(); i++)
  {
    auto* gc_label = new QLabel(tr("Port %1").arg(i + 1));
    auto* gc_box = m_gc_controller_boxes[i] = new QComboBox();
    auto* gc_button = m_gc_buttons[i] = new QPushButton(tr("Configure"));

    for (const auto& item :
         {tr("None"), tr("Standard Controller"), tr("GameCube Adapter for Wii U"),
          tr("Steering Wheel"), tr("Dance Mat"), tr("DK Bongos"), tr("GBA"), tr("Keyboard")})
    {
      gc_box->addItem(item);
    }

    int controller_row = m_gc_layout->rowCount();
    m_gc_layout->addWidget(gc_label, controller_row, 0);
    m_gc_layout->addWidget(gc_box, controller_row, 1);
    m_gc_layout->addWidget(gc_button, controller_row, 2);
  }
  m_gc_box->setLayout(m_gc_layout);
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
  hspacing = pixel_ratio * style->pixelMetric(QStyle::PM_DefaultLayoutSpacing);
  if (hspacing >= 0)
    return hspacing;

  // Ripped from qtbase/src/widgets/styles/qcommonstyle.cpp
  return pixel_ratio * 6;
}

void ControllersWindow::CreateWiimoteLayout()
{
  m_wiimote_layout = new QGridLayout();
  m_wiimote_box = new QGroupBox(tr("Wii Remotes"));
  m_wiimote_box->setLayout(m_wiimote_layout);

  m_wiimote_passthrough = new QRadioButton(tr("Passthrough a Bluetooth adapter"));
  m_wiimote_sync = new QPushButton(tr("Sync"));
  m_wiimote_reset = new QPushButton(tr("Reset"));
  m_wiimote_refresh = new QPushButton(tr("Refresh"));
  m_wiimote_pt_labels[0] = new QLabel(tr("Sync real Wii Remotes and pair them"));
  m_wiimote_pt_labels[1] = new QLabel(tr("Reset all saved Wii Remote pairings"));
  m_wiimote_emu = new QRadioButton(tr("Emulate the Wii's Bluetooth adapter"));
  m_wiimote_continuous_scanning = new QCheckBox(tr("Continuous Scanning"));
  m_wiimote_real_balance_board = new QCheckBox(tr("Real Balance Board"));
  m_wiimote_speaker_data = new QCheckBox(tr("Enable Speaker Data"));

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
    auto* wm_button = m_wiimote_buttons[i] = new QPushButton(tr("Configure"));

    for (const auto& item : {tr("None"), tr("Emulated Wii Remote"), tr("Real Wii Remote")})
      wm_box->addItem(item);

    int wm_row = m_wiimote_layout->rowCount();
    m_wiimote_layout->addWidget(wm_label, wm_row, 1);
    m_wiimote_layout->addWidget(wm_box, wm_row, 2);
    m_wiimote_layout->addWidget(wm_button, wm_row, 3);
  }

  int continuous_scanning_row = m_wiimote_layout->rowCount();
  m_wiimote_layout->addWidget(m_wiimote_continuous_scanning, continuous_scanning_row, 1, 1, 2);
  m_wiimote_layout->addWidget(m_wiimote_refresh, continuous_scanning_row, 3);

  m_wiimote_layout->addWidget(m_wiimote_real_balance_board, m_wiimote_layout->rowCount(), 1, 1, -1);
  m_wiimote_layout->addWidget(m_wiimote_speaker_data, m_wiimote_layout->rowCount(), 1, 1, -1);
}

void ControllersWindow::CreateAdvancedLayout()
{
  m_advanced_box = new QGroupBox(tr("Advanced"));
  m_advanced_layout = new QHBoxLayout();
  m_advanced_bg_input = new QCheckBox(tr("Background Input"));

  m_advanced_layout->addWidget(m_advanced_bg_input);

  m_advanced_box->setLayout(m_advanced_layout);
}

void ControllersWindow::CreateMainLayout()
{
  auto* layout = new QVBoxLayout();
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);

  layout->addWidget(m_gc_box);
  layout->addWidget(m_wiimote_box);
  layout->addWidget(m_advanced_box);
  layout->addWidget(m_button_box);

  WrapInScrollArea(this, layout);
}

void ControllersWindow::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          [=](Core::State state) { OnEmulationStateChanged(state != Core::State::Uninitialized); });

  connect(m_wiimote_passthrough, &QRadioButton::toggled, this,
          &ControllersWindow::OnWiimoteModeChanged);

  connect(m_advanced_bg_input, &QCheckBox::toggled, this, &ControllersWindow::SaveSettings);
  connect(m_wiimote_continuous_scanning, &QCheckBox::toggled, this,
          &ControllersWindow::SaveSettings);
  connect(m_wiimote_real_balance_board, &QCheckBox::toggled, this,
          &ControllersWindow::SaveSettings);
  connect(m_wiimote_speaker_data, &QCheckBox::toggled, this, &ControllersWindow::SaveSettings);
  connect(m_wiimote_sync, &QPushButton::clicked, this,
          &ControllersWindow::OnBluetoothPassthroughSyncPressed);
  connect(m_wiimote_reset, &QPushButton::clicked, this,
          &ControllersWindow::OnBluetoothPassthroughResetPressed);
  connect(m_wiimote_refresh, &QPushButton::clicked, this,
          &ControllersWindow::OnWiimoteRefreshPressed);

  for (size_t i = 0; i < m_wiimote_groups.size(); i++)
  {
    connect(m_wiimote_boxes[i],
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &ControllersWindow::SaveSettings);
    connect(m_wiimote_boxes[i],
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &ControllersWindow::OnWiimoteTypeChanged);
    connect(m_wiimote_buttons[i], &QPushButton::clicked, this,
            &ControllersWindow::OnWiimoteConfigure);

    connect(m_gc_controller_boxes[i],
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &ControllersWindow::SaveSettings);
    connect(m_gc_controller_boxes[i],
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &ControllersWindow::OnGCTypeChanged);
    connect(m_gc_buttons[i], &QPushButton::clicked, this, &ControllersWindow::OnGCPadConfigure);
  }
}

void ControllersWindow::OnWiimoteModeChanged(bool passthrough)
{
  SaveSettings();

  m_wiimote_sync->setEnabled(passthrough);
  m_wiimote_reset->setEnabled(passthrough);

  for (size_t i = 0; i < m_wiimote_groups.size(); i++)
  {
    const int index = m_wiimote_boxes[i]->currentIndex();

    if (i < 2)
      m_wiimote_pt_labels[i]->setEnabled(passthrough);

    m_wiimote_labels[i]->setEnabled(!passthrough);
    m_wiimote_boxes[i]->setEnabled(!passthrough);
    m_wiimote_buttons[i]->setEnabled(!passthrough && index != 0 && index != 2);
  }

  m_wiimote_refresh->setEnabled(!passthrough);
  m_wiimote_real_balance_board->setEnabled(!passthrough);
  m_wiimote_speaker_data->setEnabled(!passthrough);
  m_wiimote_continuous_scanning->setEnabled(!passthrough);
}

void ControllersWindow::OnWiimoteTypeChanged(int type)
{
  const auto* box = static_cast<QComboBox*>(QObject::sender());
  for (size_t i = 0; i < m_wiimote_groups.size(); i++)
  {
    if (m_wiimote_boxes[i] == box)
    {
      const int index = box->currentIndex();
      m_wiimote_buttons[i]->setEnabled(index != 0 && index != 2);
      return;
    }
  }

  SaveSettings();
}

void ControllersWindow::OnGCTypeChanged(int type)
{
  const auto* box = static_cast<QComboBox*>(QObject::sender());

  for (size_t i = 0; i < m_gc_groups.size(); i++)
  {
    if (m_gc_controller_boxes[i] == box)
    {
      const int index = box->currentIndex();
      m_gc_buttons[i]->setEnabled(index != 0 && index != 6);
      return;
    }
  }

  SaveSettings();
}

void ControllersWindow::OnBluetoothPassthroughResetPressed()
{
  const auto ios = IOS::HLE::GetIOS();

  if (!ios)
  {
    ModalMessageBox::warning(
        this, tr("Warning"),
        tr("Saved Wii Remote pairings can only be reset when a Wii game is running."));
    return;
  }

  auto device = ios->GetDeviceByName("/dev/usb/oh1/57e/305");
  if (device != nullptr)
  {
    std::static_pointer_cast<IOS::HLE::Device::BluetoothBase>(device)->TriggerSyncButtonHeldEvent();
  }
}

void ControllersWindow::OnBluetoothPassthroughSyncPressed()
{
  const auto ios = IOS::HLE::GetIOS();

  if (!ios)
  {
    ModalMessageBox::warning(this, tr("Warning"),
                             tr("A sync can only be triggered when a Wii game is running."));
    return;
  }

  auto device = ios->GetDeviceByName("/dev/usb/oh1/57e/305");

  if (device != nullptr)
  {
    std::static_pointer_cast<IOS::HLE::Device::BluetoothBase>(device)
        ->TriggerSyncButtonPressedEvent();
  }
}

void ControllersWindow::OnWiimoteRefreshPressed()
{
  WiimoteReal::Refresh();
}

void ControllersWindow::OnEmulationStateChanged(bool running)
{
  if (!SConfig::GetInstance().bWii)
  {
    m_wiimote_sync->setEnabled(!running);
    m_wiimote_reset->setEnabled(!running);

    for (size_t i = 0; i < m_wiimote_groups.size(); i++)
      m_wiimote_boxes[i]->setEnabled(!running);
  }

  m_wiimote_emu->setEnabled(!running);
  m_wiimote_passthrough->setEnabled(!running);

  if (!SConfig::GetInstance().bWii)
  {
    m_wiimote_real_balance_board->setEnabled(!running);
    m_wiimote_continuous_scanning->setEnabled(!running);
    m_wiimote_speaker_data->setEnabled(!running);
  }
}

void ControllersWindow::OnGCPadConfigure()
{
  size_t index;
  for (index = 0; index < m_gc_groups.size(); index++)
  {
    if (m_gc_buttons[index] == QObject::sender())
      break;
  }

  MappingWindow::Type type;

  switch (m_gc_controller_boxes[index]->currentIndex())
  {
  case 0:  // None
  case 6:  // GBA
    return;
  case 1:  // Standard Controller
    type = MappingWindow::Type::MAPPING_GCPAD;
    break;
  case 2:  // GameCube Adapter for Wii U
    GCPadWiiUConfigDialog(static_cast<int>(index), this).exec();
    return;
  case 3:  // Steering Wheel
    type = MappingWindow::Type::MAPPING_GC_STEERINGWHEEL;
    break;
  case 4:  // Dance Mat
    type = MappingWindow::Type::MAPPING_GC_DANCEMAT;
    break;
  case 5:  // DK Bongos
    type = MappingWindow::Type::MAPPING_GC_BONGOS;
    break;
  case 7:  // Keyboard
    type = MappingWindow::Type::MAPPING_GC_KEYBOARD;
    break;
  default:
    return;
  }

  MappingWindow* window = new MappingWindow(this, type, static_cast<int>(index));
  window->setAttribute(Qt::WA_DeleteOnClose, true);
  window->setWindowModality(Qt::WindowModality::WindowModal);
  window->show();
}

void ControllersWindow::OnWiimoteConfigure()
{
  size_t index;
  for (index = 0; index < m_wiimote_groups.size(); index++)
  {
    if (m_wiimote_buttons[index] == QObject::sender())
      break;
  }

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
  window->show();
}

void ControllersWindow::LoadSettings()
{
  for (size_t i = 0; i < m_wiimote_groups.size(); i++)
  {
    const std::optional<int> gc_index = ToGCMenuIndex(SConfig::GetInstance().m_SIDevice[i]);
    if (gc_index)
    {
      m_gc_controller_boxes[i]->setCurrentIndex(*gc_index);
      m_gc_buttons[i]->setEnabled(*gc_index != 0 && *gc_index != 6);
    }
    m_wiimote_boxes[i]->setCurrentIndex(g_wiimote_sources[i]);
    m_wiimote_buttons[i]->setEnabled(g_wiimote_sources[i] != 0 && g_wiimote_sources[i] != 2);
  }
  m_wiimote_real_balance_board->setChecked(g_wiimote_sources[WIIMOTE_BALANCE_BOARD] ==
                                           WIIMOTE_SRC_REAL);
  m_wiimote_speaker_data->setChecked(SConfig::GetInstance().m_WiimoteEnableSpeaker);
  m_wiimote_continuous_scanning->setChecked(SConfig::GetInstance().m_WiimoteContinuousScanning);

  m_advanced_bg_input->setChecked(SConfig::GetInstance().m_BackgroundInput);

  if (SConfig::GetInstance().m_bt_passthrough_enabled)
    m_wiimote_passthrough->setChecked(true);
  else
    m_wiimote_emu->setChecked(true);

  OnWiimoteModeChanged(SConfig::GetInstance().m_bt_passthrough_enabled);
}

void ControllersWindow::SaveSettings()
{
  SConfig::GetInstance().m_WiimoteEnableSpeaker = m_wiimote_speaker_data->isChecked();
  SConfig::GetInstance().m_WiimoteContinuousScanning = m_wiimote_continuous_scanning->isChecked();
  SConfig::GetInstance().m_bt_passthrough_enabled = m_wiimote_passthrough->isChecked();
  SConfig::GetInstance().m_BackgroundInput = m_advanced_bg_input->isChecked();

  WiimoteReal::ChangeWiimoteSource(WIIMOTE_BALANCE_BOARD,
                                   m_wiimote_real_balance_board->isChecked() ? WIIMOTE_SRC_REAL :
                                                                               WIIMOTE_SRC_NONE);

  for (size_t i = 0; i < m_wiimote_groups.size(); i++)
  {
    const int index = m_wiimote_boxes[i]->currentIndex();
    m_wiimote_buttons[i]->setEnabled(index != 0 && index != 2);
    WiimoteReal::ChangeWiimoteSource(static_cast<u32>(i), index);
  }

  UICommon::SaveWiimoteSources();

  for (size_t i = 0; i < m_gc_groups.size(); i++)
  {
    const int index = m_gc_controller_boxes[i]->currentIndex();
    const std::optional<SerialInterface::SIDevices> si_device = FromGCMenuIndex(index);
    if (si_device)
    {
      SConfig::GetInstance().m_SIDevice[i] = *si_device;

      if (Core::IsRunning())
        SerialInterface::ChangeDevice(*si_device, static_cast<s32>(i));
    }

    m_gc_buttons[i]->setEnabled(index != 0 && index != 6);
  }

  if (GCAdapter::UseAdapter())
    GCAdapter::StartScanThread();
  else
    GCAdapter::StopScanThread();

  SConfig::GetInstance().SaveSettings();
}
