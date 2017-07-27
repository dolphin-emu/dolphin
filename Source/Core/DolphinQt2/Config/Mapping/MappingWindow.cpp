// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include "DolphinQt2/Config/Mapping/MappingWindow.h"

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/Core.h"
#include "DolphinQt2/Config/Mapping/GCKeyboardEmu.h"
#include "DolphinQt2/Config/Mapping/GCPadEmu.h"
#include "DolphinQt2/Config/Mapping/GCPadWiiU.h"
#include "DolphinQt2/Config/Mapping/Hotkey3D.h"
#include "DolphinQt2/Config/Mapping/HotkeyGeneral.h"
#include "DolphinQt2/Config/Mapping/HotkeyGraphics.h"
#include "DolphinQt2/Config/Mapping/HotkeyStates.h"
#include "DolphinQt2/Config/Mapping/HotkeyTAS.h"
#include "DolphinQt2/Config/Mapping/HotkeyWii.h"
#include "DolphinQt2/Config/Mapping/WiimoteEmuExtension.h"
#include "DolphinQt2/Config/Mapping/WiimoteEmuGeneral.h"
#include "DolphinQt2/Config/Mapping/WiimoteEmuMotionControl.h"
#include "DolphinQt2/Settings.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/Device.h"
#include "InputCommon/InputConfig.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"

MappingWindow::MappingWindow(QWidget* parent, int port_num) : QDialog(parent), m_port(port_num)
{
  setWindowTitle(tr("Port %1").arg(port_num + 1));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  CreateDevicesLayout();
  CreateProfilesLayout();
  CreateResetLayout();
  CreateMainLayout();
  ConnectWidgets();
}

void MappingWindow::CreateDevicesLayout()
{
  m_devices_layout = new QHBoxLayout();
  m_devices_box = new QGroupBox(tr("Device"));
  m_devices_combo = new QComboBox();
  m_devices_refresh = new QPushButton(tr("Refresh"));

  m_devices_refresh->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  m_devices_layout->addWidget(m_devices_combo);
  m_devices_layout->addWidget(m_devices_refresh);

  m_devices_box->setLayout(m_devices_layout);
}

void MappingWindow::CreateProfilesLayout()
{
  m_profiles_layout = new QHBoxLayout();
  m_profiles_box = new QGroupBox(tr("Profile"));
  m_profiles_combo = new QComboBox();
  m_profiles_load = new QPushButton(tr("Load"));
  m_profiles_save = new QPushButton(tr("Save"));
  m_profiles_delete = new QPushButton(tr("Delete"));

  auto* button_layout = new QHBoxLayout();

  m_profiles_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_profiles_combo->setEditable(true);

  m_profiles_layout->addWidget(m_profiles_combo);
  button_layout->addWidget(m_profiles_load);
  button_layout->addWidget(m_profiles_save);
  button_layout->addWidget(m_profiles_delete);
  m_profiles_layout->addItem(button_layout);

  m_profiles_box->setLayout(m_profiles_layout);
}

void MappingWindow::CreateResetLayout()
{
  m_reset_layout = new QHBoxLayout();
  m_reset_box = new QGroupBox(tr("Reset"));
  m_reset_clear = new QPushButton(tr("Clear"));
  m_reset_default = new QPushButton(tr("Default"));

  m_reset_box->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  m_reset_layout->addWidget(m_reset_default);
  m_reset_layout->addWidget(m_reset_clear);

  m_reset_box->setLayout(m_reset_layout);
}

void MappingWindow::CreateMainLayout()
{
  m_main_layout = new QVBoxLayout();
  m_config_layout = new QHBoxLayout();
  m_tab_widget = new QTabWidget();
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Ok);

  m_config_layout->addWidget(m_devices_box);
  m_config_layout->addWidget(m_reset_box);
  m_config_layout->addWidget(m_profiles_box);

  m_main_layout->addItem(m_config_layout);
  m_main_layout->addWidget(m_tab_widget);
  m_main_layout->addWidget(m_button_box);

  setLayout(m_main_layout);
}

void MappingWindow::ConnectWidgets()
{
  connect(m_devices_refresh, &QPushButton::clicked, this, &MappingWindow::RefreshDevices);
  connect(m_devices_combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &MappingWindow::OnDeviceChanged);
  connect(m_reset_clear, &QPushButton::clicked, this, [this] { emit ClearFields(); });
  connect(m_reset_default, &QPushButton::clicked, this, &MappingWindow::OnDefaultFieldsPressed);
  connect(m_profiles_save, &QPushButton::clicked, this, &MappingWindow::OnSaveProfilePressed);
  connect(m_profiles_load, &QPushButton::clicked, this, &MappingWindow::OnLoadProfilePressed);
  connect(m_profiles_delete, &QPushButton::clicked, this, &MappingWindow::OnDeleteProfilePressed);
  connect(m_button_box, &QDialogButtonBox::accepted, this, &MappingWindow::accept);
}

void MappingWindow::OnDeleteProfilePressed()
{
  auto& settings = Settings::Instance();
  const QString profile_name = m_profiles_combo->currentText();
  if (!settings.GetProfiles(m_config).contains(profile_name))
  {
    QMessageBox error;
    error.setIcon(QMessageBox::Critical);
    error.setText(tr("The profile '%1' does not exist").arg(profile_name));

    error.exec();
    return;
  }

  QMessageBox confirm(this);

  confirm.setIcon(QMessageBox::Warning);
  confirm.setText(tr("Are you sure that you want to delete '%1'?").arg(profile_name));
  confirm.setInformativeText(tr("This cannot be undone!"));
  confirm.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);

  if (confirm.exec() != QMessageBox::Yes)
  {
    return;
  }

  m_profiles_combo->removeItem(m_profiles_combo->currentIndex());

  QMessageBox result(this);

  std::string profile_path = settings.GetProfileINIPath(m_config, profile_name).toStdString();

  File::CreateFullPath(profile_path);

  File::Delete(profile_path);

  result.setIcon(QMessageBox::Information);
  result.setText(tr("Successfully deleted '%1'.").arg(profile_name));
}

void MappingWindow::OnLoadProfilePressed()
{
  const QString profile_name = m_profiles_combo->currentText();

  if (profile_name.isEmpty())
    return;

  std::string profile_path =
      Settings::Instance().GetProfileINIPath(m_config, profile_name).toStdString();

  File::CreateFullPath(profile_path);

  IniFile ini;
  ini.Load(profile_path);

  m_controller->LoadConfig(ini.GetOrCreateSection("Profile"));
  m_controller->UpdateReferences(g_controller_interface);

  emit Update();

  RefreshDevices();
}

void MappingWindow::OnSaveProfilePressed()
{
  const QString profile_name = m_profiles_combo->currentText();

  if (profile_name.isEmpty())
    return;

  std::string profile_path =
      Settings::Instance().GetProfileINIPath(m_config, profile_name).toStdString();

  File::CreateFullPath(profile_path);

  IniFile ini;

  m_controller->SaveConfig(ini.GetOrCreateSection("Profile"));
  ini.Save(profile_path);

  if (m_profiles_combo->currentIndex() == 0)
  {
    m_profiles_combo->addItem(profile_name);
    m_profiles_combo->setCurrentIndex(m_profiles_combo->count() - 1);
  }
}

void MappingWindow::OnDeviceChanged(int index)
{
  const auto device = m_devices_combo->currentText().toStdString();
  m_devq.FromString(device);
  m_controller->default_device.FromString(device);
}

void MappingWindow::RefreshDevices()
{
  m_devices_combo->clear();

  Core::RunAsCPUThread([&] {
    g_controller_interface.RefreshDevices();
    m_controller->UpdateReferences(g_controller_interface);
    m_controller->UpdateDefaultDevice();

    const auto default_device = m_controller->default_device.ToString();

    m_devices_combo->addItem(QString::fromStdString(default_device));

    for (const auto& name : g_controller_interface.GetAllDeviceStrings())
    {
      if (name != default_device)
        m_devices_combo->addItem(QString::fromStdString(name));
    }

    m_devices_combo->setCurrentIndex(0);
  });
}

void MappingWindow::ChangeMappingType(MappingWindow::Type type)
{
  if (m_mapping_type == type)
    return;

  ClearWidgets();

  m_controller = nullptr;

  MappingWidget* widget;

  switch (type)
  {
  case Type::MAPPING_GC_KEYBOARD:
    widget = new GCKeyboardEmu(this);
    AddWidget(tr("GameCube Keyboard"), widget);
    setWindowTitle(tr("GameCube Keyboard at Port %1").arg(GetPort() + 1));
    break;
  case Type::MAPPING_GC_BONGOS:
  case Type::MAPPING_GC_STEERINGWHEEL:
  case Type::MAPPING_GC_DANCEMAT:
  case Type::MAPPING_GCPAD:
    widget = new GCPadEmu(this);
    setWindowTitle(tr("GameCube Controller at Port %1").arg(GetPort() + 1));
    AddWidget(tr("GameCube Controller"), widget);
    break;
  case Type::MAPPING_GCPAD_WIIU:
    widget = new GCPadWiiU(this);
    setWindowTitle(tr("GameCube Adapter for Wii U at Port %1").arg(GetPort() + 1));
    AddWidget(tr("GameCube Adapter for Wii U"), widget);
    break;
  case Type::MAPPING_WIIMOTE_EMU:
  case Type::MAPPING_WIIMOTE_HYBRID:
  {
    auto* extension = new WiimoteEmuExtension(this);
    widget = new WiimoteEmuGeneral(this, extension);
    setWindowTitle(tr("Wii Remote %1").arg(GetPort() + 1));
    AddWidget(tr("General and Options"), widget);
    AddWidget(tr("Motion Controls and IR"), new WiimoteEmuMotionControl(this));
    AddWidget(tr("Extension"), extension);
    break;
  }
  case Type::MAPPING_HOTKEYS:
  {
    widget = new HotkeyGeneral(this);
    AddWidget(tr("General"), widget);
    AddWidget(tr("TAS Tools"), new HotkeyTAS(this));
    AddWidget(tr("Wii and Wii Remote"), new HotkeyWii(this));
    AddWidget(tr("Graphics"), new HotkeyGraphics(this));
    AddWidget(tr("3D"), new Hotkey3D(this));
    AddWidget(tr("Save and Load State"), new HotkeyStates(this));
    setWindowTitle(tr("Hotkey Settings"));
    break;
  }
  default:
    return;
  }

  widget->LoadSettings();

  m_profiles_combo->clear();

  m_config = widget->GetConfig();

  if (m_config)
  {
    m_controller = m_config->GetController(GetPort());

    m_profiles_combo->addItem(QStringLiteral(""));
    for (const auto& item : Settings::Instance().GetProfiles(m_config))
      m_profiles_combo->addItem(item);
  }

  SetLayoutComplex(type != Type::MAPPING_GCPAD_WIIU);

  if (m_controller != nullptr)
    RefreshDevices();

  m_mapping_type = type;
}

void MappingWindow::ClearWidgets()
{
  m_tab_widget->clear();
}

void MappingWindow::AddWidget(const QString& name, QWidget* widget)
{
  m_tab_widget->addTab(widget, name);
}

void MappingWindow::SetLayoutComplex(bool is_complex)
{
  m_reset_box->setHidden(!is_complex);
  m_profiles_box->setHidden(!is_complex);
  m_devices_box->setHidden(!is_complex);

  m_is_complex = is_complex;
}

int MappingWindow::GetPort() const
{
  return m_port;
}

ControllerEmu::EmulatedController* MappingWindow::GetController() const
{
  return m_controller;
}

const ciface::Core::DeviceQualifier& MappingWindow::GetDeviceQualifier() const
{
  return m_devq;
}

std::shared_ptr<ciface::Core::Device> MappingWindow::GetDevice() const
{
  return g_controller_interface.FindDevice(m_devq);
}

void MappingWindow::OnDefaultFieldsPressed()
{
  if (m_controller == nullptr)
    return;

  m_controller->LoadDefaults(g_controller_interface);
  m_controller->UpdateReferences(g_controller_interface);
  emit Update();
}
