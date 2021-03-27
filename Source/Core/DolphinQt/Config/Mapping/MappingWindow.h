// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>
#include <QString>
#include <memory>

#include "InputCommon/ControllerInterface/CoreDevice.h"

namespace ControllerEmu
{
class EmulatedController;
}

class InputConfig;
class QComboBox;
class QDialogButtonBox;
class QEvent;
class QHBoxLayout;
class QGroupBox;
class QVBoxLayout;
class QPushButton;
class QTabWidget;
class QToolButton;
class QWidget;

class MappingWindow final : public QDialog
{
  Q_OBJECT
public:
  enum class Type
  {
    // GameCube
    MAPPING_GC_BONGOS,
    MAPPING_GC_DANCEMAT,
    MAPPING_GC_GBA,
    MAPPING_GC_KEYBOARD,
    MAPPING_GCPAD,
    MAPPING_GC_STEERINGWHEEL,
    MAPPING_GC_MICROPHONE,
    // Wii
    MAPPING_WIIMOTE_EMU,
    MAPPING_BALANCE_BOARD_EMU,
    // Hotkeys
    MAPPING_HOTKEYS,
    // Freelook
    MAPPING_FREELOOK,
  };

  explicit MappingWindow(QWidget* parent, Type type, int port_num);

  int GetPort() const;
  ControllerEmu::EmulatedController* GetController() const;
  bool IsMappingAllDevices() const;
  void ShowExtensionMotionTabs(bool show);

signals:
  // Emitted when config has changed so widgets can update to reflect the change.
  void ConfigChanged();
  // Emitted at 30hz for real-time indicators to be updated.
  void Update();
  void Save();

private:
  void SetMappingType(Type type);
  void CreateDevicesLayout();
  void CreateProfilesLayout();
  void CreateResetLayout();
  void CreateMainLayout();
  void ConnectWidgets();

  QWidget* AddWidget(const QString& name, QWidget* widget);

  void RefreshDevices();

  void OnSelectProfile(int index);
  void OnProfileTextChanged(const QString& text);
  void OnDeleteProfilePressed();
  void OnLoadProfilePressed();
  void OnSaveProfilePressed();
  void UpdateProfileIndex();
  void UpdateProfileButtonState();
  void PopulateProfileSelection();

  void OnDefaultFieldsPressed();
  void OnClearFieldsPressed();
  void OnSelectDevice(int index);
  void OnGlobalDevicesChanged();

  ControllerEmu::EmulatedController* m_controller = nullptr;

  // Main
  QVBoxLayout* m_main_layout;
  QHBoxLayout* m_config_layout;
  QDialogButtonBox* m_button_box;

  // Devices
  QGroupBox* m_devices_box;
  QHBoxLayout* m_devices_layout;
  QComboBox* m_devices_combo;
  QAction* m_all_devices_action;

  // Profiles
  QGroupBox* m_profiles_box;
  QHBoxLayout* m_profiles_layout;
  QComboBox* m_profiles_combo;
  QPushButton* m_profiles_load;
  QPushButton* m_profiles_save;
  QPushButton* m_profiles_delete;

  // Reset
  QGroupBox* m_reset_box;
  QHBoxLayout* m_reset_layout;
  QPushButton* m_reset_default;
  QPushButton* m_reset_clear;

  QTabWidget* m_tab_widget;
  QWidget* m_extension_motion_input_tab;
  QWidget* m_extension_motion_simulation_tab;
  const QString EXTENSION_MOTION_INPUT_TAB_NAME = tr("Extension Motion Input");
  const QString EXTENSION_MOTION_SIMULATION_TAB_NAME = tr("Extension Motion Simulation");

  Type m_mapping_type;
  const int m_port;
  InputConfig* m_config;
};
