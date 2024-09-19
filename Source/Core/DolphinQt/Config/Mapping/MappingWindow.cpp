// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/MappingWindow.h"

#include <QAction>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>

#include "Core/Core.h"
#include "Core/HotkeyManager.h"

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"

#include "DolphinQt/Config/Mapping/FreeLookGeneral.h"
#include "DolphinQt/Config/Mapping/FreeLookRotation.h"
#include "DolphinQt/Config/Mapping/GBAPadEmu.h"
#include "DolphinQt/Config/Mapping/GCKeyboardEmu.h"
#include "DolphinQt/Config/Mapping/GCMicrophone.h"
#include "DolphinQt/Config/Mapping/GCPadEmu.h"
#include "DolphinQt/Config/Mapping/Hotkey3D.h"
#include "DolphinQt/Config/Mapping/HotkeyControllerProfile.h"
#include "DolphinQt/Config/Mapping/HotkeyDebugging.h"
#include "DolphinQt/Config/Mapping/HotkeyGBA.h"
#include "DolphinQt/Config/Mapping/HotkeyGeneral.h"
#include "DolphinQt/Config/Mapping/HotkeyGraphics.h"
#include "DolphinQt/Config/Mapping/HotkeyStates.h"
#include "DolphinQt/Config/Mapping/HotkeyStatesOther.h"
#include "DolphinQt/Config/Mapping/HotkeyTAS.h"
#include "DolphinQt/Config/Mapping/HotkeyUSBEmu.h"
#include "DolphinQt/Config/Mapping/HotkeyWii.h"
#include "DolphinQt/Config/Mapping/WiimoteEmuExtension.h"
#include "DolphinQt/Config/Mapping/WiimoteEmuExtensionMotionInput.h"
#include "DolphinQt/Config/Mapping/WiimoteEmuExtensionMotionSimulation.h"
#include "DolphinQt/Config/Mapping/WiimoteEmuGeneral.h"
#include "DolphinQt/Config/Mapping/WiimoteEmuMotionControl.h"
#include "DolphinQt/Config/Mapping/WiimoteEmuMotionControlIMU.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "DolphinQt/QtUtils/WindowActivationEventFilter.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"
#include "DolphinQt/Settings.h"

#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"
#include "InputCommon/InputConfig.h"

MappingWindow::MappingWindow(QWidget* parent, Type type, int port_num)
    : QDialog(parent), m_port(port_num)
{
  setWindowTitle(tr("Port %1").arg(port_num + 1));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  CreateDevicesLayout();
  CreateProfilesLayout();
  CreateResetLayout();
  CreateMainLayout();
  ConnectWidgets();
  SetMappingType(type);

  const auto timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, [this] {
    const auto lock = GetController()->GetStateLock();
    emit Update();
  });

  timer->start(1000 / INDICATOR_UPDATE_FREQ);

  const auto lock = GetController()->GetStateLock();
  emit ConfigChanged();

  auto* filter = new WindowActivationEventFilter(this);
  installEventFilter(filter);

  filter->connect(filter, &WindowActivationEventFilter::windowDeactivated,
                  [] { HotkeyManagerEmu::Enable(true); });
  filter->connect(filter, &WindowActivationEventFilter::windowActivated,
                  [] { HotkeyManagerEmu::Enable(false); });
}

void MappingWindow::CreateDevicesLayout()
{
  m_devices_layout = new QHBoxLayout();
  m_devices_box = new QGroupBox(tr("Device"));
  m_devices_combo = new QComboBox();

  auto* const options = new QToolButton();
  // Make it more apparent that this is a menu with more options.
  options->setPopupMode(QToolButton::ToolButtonPopupMode::MenuButtonPopup);

  const auto refresh_action = new QAction(tr("Refresh"), options);
  connect(refresh_action, &QAction::triggered, this, &MappingWindow::RefreshDevices);

  m_all_devices_action = new QAction(tr("Create mappings for other devices"), options);
  m_all_devices_action->setCheckable(true);

  options->addAction(refresh_action);
  options->addAction(m_all_devices_action);
  options->setDefaultAction(refresh_action);

  m_devices_combo->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
  options->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  m_devices_layout->addWidget(m_devices_combo);
  m_devices_layout->addWidget(options);

  m_devices_box->setLayout(m_devices_layout);
}

void MappingWindow::CreateProfilesLayout()
{
  m_profiles_layout = new QHBoxLayout();
  m_profiles_box = new QGroupBox(tr("Profile"));
  m_profiles_combo = new QComboBox();
  m_profiles_load = new NonDefaultQPushButton(tr("Load"));
  m_profiles_save = new NonDefaultQPushButton(tr("Save"));
  m_profiles_delete = new NonDefaultQPushButton(tr("Delete"));

  auto* button_layout = new QHBoxLayout();

  m_profiles_combo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  m_profiles_combo->setMinimumWidth(100);
  m_profiles_combo->setEditable(true);

  m_profiles_layout->addWidget(m_profiles_combo);
  button_layout->addWidget(m_profiles_load);
  button_layout->addWidget(m_profiles_save);
  button_layout->addWidget(m_profiles_delete);
  m_profiles_layout->addLayout(button_layout);

  m_profiles_box->setLayout(m_profiles_layout);
}

void MappingWindow::CreateResetLayout()
{
  m_reset_layout = new QHBoxLayout();
  m_reset_box = new QGroupBox(tr("Reset"));
  m_reset_clear = new NonDefaultQPushButton(tr("Clear"));
  m_reset_default = new NonDefaultQPushButton(tr("Default"));

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
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);

  m_config_layout->addWidget(m_devices_box);
  m_config_layout->addWidget(m_reset_box);
  m_config_layout->addWidget(m_profiles_box);

  m_main_layout->addLayout(m_config_layout);
  m_main_layout->addWidget(m_tab_widget);
  m_main_layout->addWidget(m_button_box);

  setLayout(m_main_layout);
}

void MappingWindow::ConnectWidgets()
{
  connect(&Settings::Instance(), &Settings::DevicesChanged, this,
          &MappingWindow::OnGlobalDevicesChanged);
  connect(this, &MappingWindow::ConfigChanged, this, &MappingWindow::OnGlobalDevicesChanged);
  connect(m_devices_combo, &QComboBox::currentIndexChanged, this, &MappingWindow::OnSelectDevice);

  connect(m_reset_clear, &QPushButton::clicked, this, &MappingWindow::OnClearFieldsPressed);
  connect(m_reset_default, &QPushButton::clicked, this, &MappingWindow::OnDefaultFieldsPressed);
  connect(m_profiles_save, &QPushButton::clicked, this, &MappingWindow::OnSaveProfilePressed);
  connect(m_profiles_load, &QPushButton::clicked, this, &MappingWindow::OnLoadProfilePressed);
  connect(m_profiles_delete, &QPushButton::clicked, this, &MappingWindow::OnDeleteProfilePressed);

  connect(m_profiles_combo, &QComboBox::currentIndexChanged, this, &MappingWindow::OnSelectProfile);
  connect(m_profiles_combo, &QComboBox::editTextChanged, this,
          &MappingWindow::OnProfileTextChanged);

  // We currently use the "Close" button as an "Accept" button so we must save on reject.
  connect(this, &QDialog::rejected, [this] { emit Save(); });
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void MappingWindow::UpdateProfileIndex()
{
  // Make sure currentIndex and currentData are accurate when the user manually types a name.

  const auto current_text = m_profiles_combo->currentText();
  const int text_index = m_profiles_combo->findText(current_text);
  m_profiles_combo->setCurrentIndex(text_index);

  if (text_index == -1)
    m_profiles_combo->setCurrentText(current_text);
}

void MappingWindow::UpdateProfileButtonState()
{
  // Make sure save/delete buttons are disabled for built-in profiles

  bool builtin = false;
  if (m_profiles_combo->findText(m_profiles_combo->currentText()) != -1)
  {
    const QString profile_path = m_profiles_combo->currentData().toString();
    std::string sys_dir = File::GetSysDirectory();
    sys_dir = ReplaceAll(sys_dir, "\\", DIR_SEP);
    builtin = profile_path.startsWith(QString::fromStdString(sys_dir));
  }

  m_profiles_save->setEnabled(!builtin);
  m_profiles_delete->setEnabled(!builtin);
}

void MappingWindow::OnSelectProfile(int)
{
  UpdateProfileButtonState();
}

void MappingWindow::OnProfileTextChanged(const QString&)
{
  UpdateProfileButtonState();
}

void MappingWindow::OnDeleteProfilePressed()
{
  UpdateProfileIndex();

  const QString profile_name = m_profiles_combo->currentText();
  const QString profile_path = m_profiles_combo->currentData().toString();

  if (m_profiles_combo->currentIndex() == -1 || !File::Exists(profile_path.toStdString()))
  {
    ModalMessageBox error(this);
    error.setIcon(QMessageBox::Critical);
    error.setWindowTitle(tr("Error"));
    error.setText(tr("The profile '%1' does not exist").arg(profile_name));
    SetQWidgetWindowDecorations(&error);
    error.exec();
    return;
  }

  ModalMessageBox confirm(this);

  confirm.setIcon(QMessageBox::Warning);
  confirm.setWindowTitle(tr("Confirm"));
  confirm.setText(tr("Are you sure that you want to delete '%1'?").arg(profile_name));
  confirm.setInformativeText(tr("This cannot be undone!"));
  confirm.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);

  SetQWidgetWindowDecorations(&confirm);
  if (confirm.exec() != QMessageBox::Yes)
  {
    return;
  }

  m_profiles_combo->removeItem(m_profiles_combo->currentIndex());
  m_profiles_combo->setCurrentIndex(-1);

  File::Delete(profile_path.toStdString());

  ModalMessageBox result(this);
  result.setIcon(QMessageBox::Information);
  result.setWindowModality(Qt::WindowModal);
  result.setWindowTitle(tr("Success"));
  result.setText(tr("Successfully deleted '%1'.").arg(profile_name));
}

void MappingWindow::OnLoadProfilePressed()
{
  UpdateProfileIndex();

  if (m_profiles_combo->currentIndex() == -1)
  {
    ModalMessageBox error(this);
    error.setIcon(QMessageBox::Critical);
    error.setWindowTitle(tr("Error"));
    error.setText(tr("The profile '%1' does not exist").arg(m_profiles_combo->currentText()));
    SetQWidgetWindowDecorations(&error);
    error.exec();
    return;
  }

  const QString profile_path = m_profiles_combo->currentData().toString();

  Common::IniFile ini;
  ini.Load(profile_path.toStdString());

  m_controller->LoadConfig(ini.GetOrCreateSection("Profile"));
  m_controller->UpdateReferences(g_controller_interface);

  const auto lock = GetController()->GetStateLock();
  emit ConfigChanged();
}

void MappingWindow::OnSaveProfilePressed()
{
  const QString profile_name = m_profiles_combo->currentText();

  if (profile_name.isEmpty())
    return;

  const std::string profile_path =
      m_config->GetUserProfileDirectoryPath() + profile_name.toStdString() + ".ini";

  File::CreateFullPath(profile_path);

  Common::IniFile ini;

  m_controller->SaveConfig(ini.GetOrCreateSection("Profile"));
  ini.Save(profile_path);

  if (m_profiles_combo->findText(profile_name) == -1)
  {
    PopulateProfileSelection();
    m_profiles_combo->setCurrentIndex(m_profiles_combo->findText(profile_name));
  }
}

void MappingWindow::OnSelectDevice(int)
{
  // Original string is stored in the "user-data".
  const auto device = m_devices_combo->currentData().toString().toStdString();

  m_controller->SetDefaultDevice(device);
  m_controller->UpdateReferences(g_controller_interface);
}

bool MappingWindow::IsMappingAllDevices() const
{
  return m_all_devices_action->isChecked();
}

void MappingWindow::RefreshDevices()
{
  g_controller_interface.RefreshDevices();
}

void MappingWindow::OnGlobalDevicesChanged()
{
  const QSignalBlocker blocker(m_devices_combo);

  m_devices_combo->clear();

  for (const auto& name : g_controller_interface.GetAllDeviceStrings())
  {
    const auto qname = QString::fromStdString(name);
    m_devices_combo->addItem(qname, qname);
  }

  const auto default_device = m_controller->GetDefaultDevice().ToString();

  if (!default_device.empty())
  {
    const auto default_device_index =
        m_devices_combo->findText(QString::fromStdString(default_device));

    if (default_device_index != -1)
    {
      m_devices_combo->setCurrentIndex(default_device_index);
    }
    else
    {
      // Selected device is not currently attached.
      m_devices_combo->insertSeparator(m_devices_combo->count());
      const auto qname = QString::fromStdString(default_device);
      m_devices_combo->addItem(QLatin1Char{'['} + tr("disconnected") + QStringLiteral("] ") + qname,
                               qname);
      m_devices_combo->setCurrentIndex(m_devices_combo->count() - 1);
    }
  }
}

void MappingWindow::SetMappingType(MappingWindow::Type type)
{
  MappingWidget* widget;

  switch (type)
  {
  case Type::MAPPING_GC_GBA:
    widget = new GBAPadEmu(this);
    setWindowTitle(tr("Game Boy Advance at Port %1").arg(GetPort() + 1));
    AddWidget(tr("Game Boy Advance"), widget);
    break;
  case Type::MAPPING_GC_KEYBOARD:
    widget = new GCKeyboardEmu(this);
    setWindowTitle(tr("GameCube Keyboard at Port %1").arg(GetPort() + 1));
    AddWidget(tr("GameCube Keyboard"), widget);
    break;
  case Type::MAPPING_GC_BONGOS:
  case Type::MAPPING_GC_STEERINGWHEEL:
  case Type::MAPPING_GC_DANCEMAT:
  case Type::MAPPING_GCPAD:
    widget = new GCPadEmu(this);
    setWindowTitle(tr("GameCube Controller at Port %1").arg(GetPort() + 1));
    AddWidget(tr("GameCube Controller"), widget);
    break;
  case Type::MAPPING_GC_MICROPHONE:
    widget = new GCMicrophone(this);
    setWindowTitle(tr("GameCube Microphone Slot %1")
                       .arg(GetPort() == 0 ? QLatin1Char{'A'} : QLatin1Char{'B'}));
    AddWidget(tr("Microphone"), widget);
    break;
  case Type::MAPPING_WIIMOTE_EMU:
  {
    auto* extension = new WiimoteEmuExtension(this);
    auto* extension_motion_input = new WiimoteEmuExtensionMotionInput(this);
    auto* extension_motion_simulation = new WiimoteEmuExtensionMotionSimulation(this);
    widget = new WiimoteEmuGeneral(this, extension);
    setWindowTitle(tr("Wii Remote %1").arg(GetPort() + 1));
    AddWidget(tr("General and Options"), widget);
    AddWidget(tr("Motion Simulation"), new WiimoteEmuMotionControl(this));
    AddWidget(tr("Motion Input"), new WiimoteEmuMotionControlIMU(this));
    AddWidget(tr("Extension"), extension);
    m_extension_motion_simulation_tab =
        AddWidget(EXTENSION_MOTION_SIMULATION_TAB_NAME, extension_motion_simulation);
    m_extension_motion_input_tab =
        AddWidget(EXTENSION_MOTION_INPUT_TAB_NAME, extension_motion_input);
    // Hide tabs by default. "Nunchuk" selection triggers an event to show them.
    ShowExtensionMotionTabs(false);
    break;
  }
  case Type::MAPPING_HOTKEYS:
  {
    widget = new HotkeyGeneral(this);
    AddWidget(tr("General"), widget);
    // i18n: TAS is short for tool-assisted speedrun. Read http://tasvideos.org/ for details.
    // Frame advance is an example of a typical TAS tool.
    AddWidget(tr("TAS Tools"), new HotkeyTAS(this));

    AddWidget(tr("Debugging"), new HotkeyDebugging(this));

    AddWidget(tr("Wii and Wii Remote"), new HotkeyWii(this));
    AddWidget(tr("Controller Profile"), new HotkeyControllerProfile(this));
    AddWidget(tr("Graphics"), new HotkeyGraphics(this));
    AddWidget(tr("USB Emulation"), new HotkeyUSBEmu(this));
    // i18n: Stereoscopic 3D
    AddWidget(tr("3D"), new Hotkey3D(this));
    AddWidget(tr("Save and Load State"), new HotkeyStates(this));
    AddWidget(tr("Other State Management"), new HotkeyStatesOther(this));
    AddWidget(tr("Game Boy Advance"), new HotkeyGBA(this));
    setWindowTitle(tr("Hotkey Settings"));
    break;
  }
  case Type::MAPPING_FREELOOK:
  {
    widget = new FreeLookGeneral(this);
    AddWidget(tr("General"), widget);
    AddWidget(tr("Rotation"), new FreeLookRotation(this));
    setWindowTitle(tr("Free Look Controller %1").arg(GetPort() + 1));
  }
  break;
  default:
    return;
  }

  widget->LoadSettings();

  m_config = widget->GetConfig();

  m_controller = m_config->GetController(GetPort());

  PopulateProfileSelection();
}

void MappingWindow::PopulateProfileSelection()
{
  m_profiles_combo->clear();

  const std::string profiles_path = m_config->GetUserProfileDirectoryPath();
  for (const auto& filename : Common::DoFileSearch({profiles_path}, {".ini"}))
  {
    std::string basename;
    SplitPath(filename, nullptr, &basename, nullptr);
    if (!basename.empty())  // Ignore files with an empty name to avoid multiple problems
      m_profiles_combo->addItem(QString::fromStdString(basename), QString::fromStdString(filename));
  }

  m_profiles_combo->insertSeparator(m_profiles_combo->count());

  for (const auto& filename :
       Common::DoFileSearch({m_config->GetSysProfileDirectoryPath()}, {".ini"}))
  {
    std::string basename;
    SplitPath(filename, nullptr, &basename, nullptr);
    if (!basename.empty())
    {
      // i18n: "Stock" refers to input profiles included with Dolphin
      m_profiles_combo->addItem(tr("%1 (Stock)").arg(QString::fromStdString(basename)),
                                QString::fromStdString(filename));
    }
  }

  m_profiles_combo->setCurrentIndex(-1);
}

QWidget* MappingWindow::AddWidget(const QString& name, QWidget* widget)
{
  QWidget* wrapper = GetWrappedWidget(widget, this, 150, 210);
  m_tab_widget->addTab(wrapper, name);
  return wrapper;
}

int MappingWindow::GetPort() const
{
  return m_port;
}

ControllerEmu::EmulatedController* MappingWindow::GetController() const
{
  return m_controller;
}

void MappingWindow::OnDefaultFieldsPressed()
{
  m_controller->LoadDefaults(g_controller_interface);
  m_controller->UpdateReferences(g_controller_interface);

  const auto lock = GetController()->GetStateLock();
  emit ConfigChanged();
  emit Save();
}

void MappingWindow::OnClearFieldsPressed()
{
  // Loading an empty inifile section clears everything.
  Common::IniFile::Section sec;

  // Keep the currently selected device.
  const auto default_device = m_controller->GetDefaultDevice();
  m_controller->LoadConfig(&sec);
  m_controller->SetDefaultDevice(default_device);

  m_controller->UpdateReferences(g_controller_interface);

  const auto lock = GetController()->GetStateLock();
  emit ConfigChanged();
  emit Save();
}

void MappingWindow::ShowExtensionMotionTabs(bool show)
{
  if (show)
  {
    m_tab_widget->addTab(m_extension_motion_simulation_tab, EXTENSION_MOTION_SIMULATION_TAB_NAME);
    m_tab_widget->addTab(m_extension_motion_input_tab, EXTENSION_MOTION_INPUT_TAB_NAME);
  }
  else
  {
    m_tab_widget->removeTab(5);
    m_tab_widget->removeTab(4);
  }
}
