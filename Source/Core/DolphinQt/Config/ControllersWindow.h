// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

#include <array>

class CommonControllersWidget;
class MappingWindow;
class QDialogButtonBox;
class QCheckBox;
class QComboBox;
class QHBoxLayout;
class QGridLayout;
class QGroupBox;
class QLabel;
class QPushButton;
class QRadioButton;
class WiimoteControllersWidget;

class ControllersWindow final : public QDialog
{
  Q_OBJECT
public:
  explicit ControllersWindow(QWidget* parent);

private:
  void OnGCTypeChanged(int state);
  void SaveSettings();
  void OnGCPadConfigure();

  void CreateGamecubeLayout();
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
  WiimoteControllersWidget* m_wiimote_controllers;

  // Common
  CommonControllersWidget* m_common;
};
