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
  void OnEmulationStateChanged(bool running);
  void OnWiimoteModeChanged(bool passthrough);
  void OnWiimoteTypeChanged(int state);
  void OnGCTypeChanged(int state);
  void SaveSettings();
  void UnimplementedButton();
  void OnBluetoothPassthroughSyncPressed();
  void OnBluetoothPassthroughResetPressed();
  void OnWiimoteRefreshPressed();
  void OnGCPadConfigure();
  void OnWiimoteConfigure();

  void CreateGamecubeLayout();
  void CreateWiimoteLayout();
  void CreateAdvancedLayout();
  void CreateMainLayout();
  void ConnectWidgets();
  void LoadSettings();

  // Main
  QVBoxLayout* m_main_layout;
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
  QPushButton* m_wiimote_refresh;

  // Advanced
  QGroupBox* m_advanced_box;
  QHBoxLayout* m_advanced_layout;
  QCheckBox* m_advanced_bg_input;
};
