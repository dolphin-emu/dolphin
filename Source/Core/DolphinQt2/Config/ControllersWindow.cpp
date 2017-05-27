// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpacerItem>
#include <QVBoxLayout>

#include <unordered_map>

#include "Core/ConfigManager.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Bluetooth/BTReal.h"
#include "Core/NetPlayProto.h"
#include "DolphinQt2/Config/Mapping/MappingWindow.h"
#include "DolphinQt2/Settings.h"
#include "UICommon/UICommon.h"

#include "DolphinQt2/Config/ControllersWindow.h"

static const std::unordered_map<SerialInterface::SIDevices, int> s_gc_types = {
    {SerialInterface::SIDEVICE_NONE, 0},         {SerialInterface::SIDEVICE_GC_CONTROLLER, 1},
    {SerialInterface::SIDEVICE_WIIU_ADAPTER, 2}, {SerialInterface::SIDEVICE_GC_STEERING, 3},
    {SerialInterface::SIDEVICE_DANCEMAT, 4},     {SerialInterface::SIDEVICE_GC_TARUKONGA, 5},
    {SerialInterface::SIDEVICE_GC_GBA, 6},       {SerialInterface::SIDEVICE_GC_KEYBOARD, 7}};

static const std::unordered_map<int, int> s_wiimote_types = {
    {WIIMOTE_SRC_NONE, 0}, {WIIMOTE_SRC_EMU, 1}, {WIIMOTE_SRC_REAL, 2}, {WIIMOTE_SRC_HYBRID, 3}};

static int ToGCMenuIndex(const SerialInterface::SIDevices sidevice)
{
  return s_gc_types.at(sidevice);
}

static SerialInterface::SIDevices FromGCMenuIndex(const int menudevice)
{
  auto it = std::find_if(s_gc_types.begin(), s_gc_types.end(),
                         [=](auto pair) { return pair.second == menudevice; });
  return it->first;
}

static int ToWiimoteMenuIndex(const int device)
{
  return s_wiimote_types.at(device);
}

static int FromWiimoteMenuIndex(const int menudevice)
{
  auto it = std::find_if(s_wiimote_types.begin(), s_wiimote_types.end(),
                         [=](auto pair) { return pair.second == menudevice; });
  return it->first;
}

ControllersWindow::ControllersWindow(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Controller Settings"));

  CreateGamecubeLayout();
  CreateWiimoteLayout();
  CreateAdvancedLayout();
  CreateMainLayout();
  LoadSettings();
  ConnectWidgets();

  for (size_t i = 0; i < m_gc_mappings.size(); i++)
    m_gc_mappings[i] = new MappingWindow(this, static_cast<int>(i));

  for (size_t i = 0; i < m_wiimote_mappings.size(); i++)
    m_wiimote_mappings[i] = new MappingWindow(this, static_cast<int>(i));
}

void ControllersWindow::CreateGamecubeLayout()
{
  m_gc_box = new QGroupBox(tr("GameCube Controllers"));
  m_gc_layout = new QFormLayout();

  for (size_t i = 0; i < m_gc_groups.size(); i++)
  {
    auto* gc_box = m_gc_controller_boxes[i] = new QComboBox();
    auto* gc_button = m_gc_buttons[i] = new QPushButton(tr("Configure"));
    auto* gc_group = m_gc_groups[i] = new QHBoxLayout();

    gc_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    gc_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    for (const auto& item :
         {tr("None"), tr("Standard Controller"), tr("GameCube Adapter for Wii U"),
          tr("Steering Wheel"), tr("Dance Mat"), tr("DK Bongos"), tr("GBA"), tr("Keyboard")})
    {
      gc_box->addItem(item);
    }

    gc_group->addItem(new QSpacerItem(42, 1));
    gc_group->addWidget(gc_box);
    gc_group->addWidget(gc_button);

    m_gc_layout->addRow(tr("Controller %1").arg(i + 1), gc_group);
  }
  m_gc_box->setLayout(m_gc_layout);
}

static QHBoxLayout* CreateSubItem(QWidget* label, QWidget* widget)
{
  QHBoxLayout* hbox = new QHBoxLayout();
  hbox->addItem(new QSpacerItem(25, 1));

  if (label != nullptr)
    hbox->addWidget(label);

  if (widget != nullptr)
    hbox->addWidget(widget);

  return hbox;
}

static QHBoxLayout* CreateSubItem(QWidget* label, QLayoutItem* item)
{
  QHBoxLayout* hbox = new QHBoxLayout();
  hbox->addItem(new QSpacerItem(25, 1));

  if (label != nullptr)
    hbox->addWidget(label);

  if (item != nullptr)
    hbox->addItem(item);

  return hbox;
}

void ControllersWindow::CreateWiimoteLayout()
{
  m_wiimote_box = new QGroupBox(tr("Wii Remotes"));
  m_wiimote_layout = new QFormLayout();
  m_wiimote_passthrough = new QRadioButton(tr("Use Bluetooth Passthrough"));
  m_wiimote_sync = new QPushButton(tr("Sync"));
  m_wiimote_reset = new QPushButton(tr("Reset"));
  m_wiimote_refresh = new QPushButton(tr("Refresh"));
  m_wiimote_pt_labels[0] = new QLabel(tr("Sync real Wii Remotes and pair them"));
  m_wiimote_pt_labels[1] = new QLabel(tr("Reset all saved Wii Remote pairings"));
  m_wiimote_emu = new QRadioButton(tr("Emulate the Wii Bluetooth Adapter"));
  m_wiimote_continuous_scanning = new QCheckBox(tr("Continuous Scanning"));
  m_wiimote_real_balance_board = new QCheckBox(tr("Real Balance Board"));
  m_wiimote_speaker_data = new QCheckBox(tr("Enable Speaker Data"));

  m_wiimote_layout->setLabelAlignment(Qt::AlignLeft);

  // Passthrough BT

  m_wiimote_layout->addRow(m_wiimote_passthrough);

  m_wiimote_sync->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  m_wiimote_reset->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  m_wiimote_refresh->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  m_wiimote_layout->addRow(CreateSubItem(m_wiimote_pt_labels[0], m_wiimote_sync));
  m_wiimote_layout->addRow(CreateSubItem(m_wiimote_pt_labels[1], m_wiimote_reset));

  // Emulated BT
  m_wiimote_layout->addRow(m_wiimote_emu);

  for (size_t i = 0; i < m_wiimote_groups.size(); i++)
  {
    auto* wm_label = m_wiimote_labels[i] = new QLabel(tr("Wii Remote %1").arg(i + 1));
    auto* wm_box = m_wiimote_boxes[i] = new QComboBox();
    auto* wm_button = m_wiimote_buttons[i] = new QPushButton(tr("Configure"));
    auto* wm_group = m_wiimote_groups[i] = new QHBoxLayout();

    for (const auto& item :
         {tr("None"), tr("Emulated Wii Remote"), tr("Real Wii Remote"), tr("Hybrid Wii Remote")})
    {
      wm_box->addItem(item);
    }

    wm_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    wm_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    wm_group->addItem(new QSpacerItem(25, 1));
    wm_group->addWidget(wm_label);
    wm_group->addItem(new QSpacerItem(10, 1));
    wm_group->addWidget(wm_box);
    wm_group->addWidget(wm_button);

    m_wiimote_layout->addRow(wm_group);
  }

  m_wiimote_layout->addRow(CreateSubItem(m_wiimote_continuous_scanning, m_wiimote_refresh));
  m_wiimote_layout->addRow(CreateSubItem(nullptr, m_wiimote_real_balance_board));
  m_wiimote_layout->addRow(CreateSubItem(m_wiimote_speaker_data, new QSpacerItem(1, 35)));

  m_wiimote_box->setLayout(m_wiimote_layout);
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
  m_main_layout = new QVBoxLayout();
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Ok);

  m_main_layout->addWidget(m_gc_box);
  m_main_layout->addWidget(m_wiimote_box);
  m_main_layout->addWidget(m_advanced_box);
  m_main_layout->addWidget(m_button_box);

  setLayout(m_main_layout);
}

void ControllersWindow::ConnectWidgets()
{
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
  connect(m_button_box, &QDialogButtonBox::accepted, this, &ControllersWindow::accept);

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
}

void ControllersWindow::OnBluetoothPassthroughResetPressed()
{
  const auto ios = IOS::HLE::GetIOS();

  if (!ios)
  {
    QMessageBox error(this);
    error.setIcon(QMessageBox::Warning);
    error.setText(tr("Saved Wii Remote pairings can only be reset when a Wii game is running."));
    error.exec();
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
    QMessageBox error(this);
    error.setIcon(QMessageBox::Warning);
    error.setText(tr("A sync can only be triggered when a Wii game is running"));
    error.exec();
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
  if (!Settings().IsWiiGameRunning() || NetPlay::IsNetPlayRunning())
  {
    m_wiimote_sync->setEnabled(!running);
    m_wiimote_reset->setEnabled(!running);

    for (size_t i = 0; i < m_wiimote_groups.size(); i++)
      m_wiimote_boxes[i]->setEnabled(!running);
  }

  m_wiimote_emu->setEnabled(!running);
  m_wiimote_passthrough->setEnabled(!running);

  if (!Settings().IsWiiGameRunning())
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
    type = MappingWindow::Type::MAPPING_GCPAD_WIIU;
    break;
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
  m_gc_mappings[index]->ChangeMappingType(type);
  m_gc_mappings[index]->exec();
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
  case 3:  // Hybrid Wii Remote
    type = MappingWindow::Type::MAPPING_WIIMOTE_HYBRID;
    break;
  default:
    return;
  }
  m_wiimote_mappings[index]->ChangeMappingType(type);
  m_wiimote_mappings[index]->exec();
}

void ControllersWindow::UnimplementedButton()
{
  QMessageBox error_dialog(this);

  error_dialog.setIcon(QMessageBox::Warning);
  error_dialog.setText(tr("Not implemented yet."));
  error_dialog.exec();
}

void ControllersWindow::LoadSettings()
{
  for (size_t i = 0; i < m_wiimote_groups.size(); i++)
  {
    m_gc_controller_boxes[i]->setCurrentIndex(ToGCMenuIndex(Settings().GetSIDevice(i)));
    m_wiimote_boxes[i]->setCurrentIndex(ToWiimoteMenuIndex(g_wiimote_sources[i]));
  }
  m_wiimote_real_balance_board->setChecked(g_wiimote_sources[WIIMOTE_BALANCE_BOARD] ==
                                           WIIMOTE_SRC_REAL);
  m_wiimote_speaker_data->setChecked(Settings().IsWiimoteSpeakerEnabled());
  m_wiimote_continuous_scanning->setChecked(Settings().IsContinuousScanningEnabled());

  m_advanced_bg_input->setChecked(Settings().IsBackgroundInputEnabled());

  if (Settings().IsBluetoothPassthroughEnabled())
    m_wiimote_passthrough->setChecked(true);
  else
    m_wiimote_emu->setChecked(true);

  OnWiimoteModeChanged(Settings().IsBluetoothPassthroughEnabled());
}

void ControllersWindow::SaveSettings()
{
  Settings().SetWiimoteSpeakerEnabled(m_wiimote_speaker_data->isChecked());
  Settings().SetContinuousScanningEnabled(m_wiimote_continuous_scanning->isChecked());

  Settings().SetBluetoothPassthroughEnabled(m_wiimote_passthrough->isChecked());
  Settings().SetBackgroundInputEnabled(m_advanced_bg_input->isChecked());

  WiimoteReal::ChangeWiimoteSource(WIIMOTE_BALANCE_BOARD,
                                   m_wiimote_real_balance_board->isChecked() ? WIIMOTE_SRC_REAL :
                                                                               WIIMOTE_SRC_NONE);

  for (size_t i = 0; i < m_wiimote_groups.size(); i++)
  {
    const int index = m_wiimote_boxes[i]->currentIndex();
    g_wiimote_sources[i] = FromWiimoteMenuIndex(index);
    m_wiimote_buttons[i]->setEnabled(index != 0 && index != 2);
  }

  UICommon::SaveWiimoteSources();

  for (size_t i = 0; i < m_gc_groups.size(); i++)
  {
    const int index = m_gc_controller_boxes[i]->currentIndex();
    Settings().SetSIDevice(i, FromGCMenuIndex(index));
    m_gc_buttons[i]->setEnabled(index != 0 && index != 6);
  }
  Settings().Save();
}
