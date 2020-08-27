// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

#include <array>

class MappingWindow;
class QDialogButtonBox;
class QCheckBox;
class QComboBox;
class QHBoxLayout;
class QGridLayout;
class QGroupBox;
class QLabel;
class QVBoxLayout;
class QPushButton;
class QRadioButton;

class ControllersWindow final : public QDialog
{
  Q_OBJECT
public:
  explicit ControllersWindow(QWidget* parent);

private:
  void OnWiimoteModeChanged();
  void UpdateDisabledWiimoteControls();
  void OnGCTypeChanged(int state);
  void SaveSettings();
  void OnBluetoothPassthroughSyncPressed();
  void OnBluetoothPassthroughResetPressed();
  void OnWiimoteRefreshPressed();
  void OnGCPadConfigure();
  void OnWiimoteConfigure();
  void OnControllerInterfaceConfigure();

  void CreateGamecubeLayout();
  void CreateWiimoteLayout();
  void CreateCommonLayout();
  void CreateMainLayout();
  void ConnectWidgets();
  void LoadSettings();

  // Main
  QDialogButtonBox* m_button_box;

  // Gamecube
  QGroupBox* m_gc_box;
  QGridLayout* m_gc_layout;
  std::array<QComboBox*, 4> m_gc_controller_boxes;
  std::array<QPushButton*, 4> m_gc_buttons;
  std::array<QHBoxLayout*, 4> m_gc_groups;

  // Wii Remote
  QGroupBox* m_wiimote_box;
  QGridLayout* m_wiimote_layout;
  std::array<QLabel*, 4> m_wiimote_labels;
  std::array<QComboBox*, 4> m_wiimote_boxes;
  std::array<QPushButton*, 4> m_wiimote_buttons;
  std::array<QHBoxLayout*, 4> m_wiimote_groups;
  std::array<QLabel*, 2> m_wiimote_pt_labels;

  QRadioButton* m_wiimote_emu;
  QRadioButton* m_wiimote_passthrough;
  QPushButton* m_wiimote_sync;
  QPushButton* m_wiimote_reset;
  QCheckBox* m_wiimote_continuous_scanning;
  QCheckBox* m_wiimote_real_balance_board;
  QCheckBox* m_wiimote_speaker_data;
  QCheckBox* m_wiimote_ciface;
  QPushButton* m_wiimote_refresh;

  // Common
  QGroupBox* m_common_box;
  QVBoxLayout* m_common_layout;
  QCheckBox* m_common_bg_input;
  QPushButton* m_common_configure_controller_interface;
};
