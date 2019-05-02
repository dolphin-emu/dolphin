// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>
#include <QString>

#include "Common/Flag.h"
#include "InputCommon/ControllerInterface/Device.h"

class ControlReference;
class QAbstractButton;
class QComboBox;
class QDialogButtonBox;
class QListWidget;
class QVBoxLayout;
class QWidget;
class QPlainTextEdit;
class QPushButton;
class QSlider;
class QSpinBox;

namespace ControllerEmu
{
class EmulatedController;
}

class IOWindow final : public QDialog
{
  Q_OBJECT
public:
  enum class Type
  {
    Input,
    Output
  };

  explicit IOWindow(QWidget* parent, ControllerEmu::EmulatedController* m_controller,
                    ControlReference* ref, Type type);

private:
  void CreateMainLayout();
  void ConnectWidgets();
  void ConfigChanged();

  void OnDialogButtonPressed(QAbstractButton* button);
  void OnDeviceChanged(const QString& device);
  void OnDetectButtonPressed();
  void OnTestButtonPressed();
  void OnRangeChanged(int range);

  void AppendSelectedOption(const std::string& prefix);
  void UpdateOptionList();
  void UpdateDeviceList();

  // Main Layout
  QVBoxLayout* m_main_layout;

  // Devices
  QComboBox* m_devices_combo;

  // Options
  QListWidget* m_option_list;

  // Range
  QSlider* m_range_slider;
  QSpinBox* m_range_spinbox;

  // Shared actions
  QPushButton* m_select_button;
  QPushButton* m_or_button;

  // Input actions
  QPushButton* m_detect_button;
  QPushButton* m_and_button;
  QPushButton* m_not_button;
  QPushButton* m_add_button;

  // Output actions
  QPushButton* m_test_button;

  // Textarea
  QPlainTextEdit* m_expression_text;

  // Buttonbox
  QDialogButtonBox* m_button_box;
  QPushButton* m_clear_button;
  QPushButton* m_apply_button;

  ControlReference* m_reference;
  ControllerEmu::EmulatedController* m_controller;

  ciface::Core::DeviceQualifier m_devq;
  Type m_type;
};
