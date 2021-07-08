// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <mutex>
#include <string>

#include <QComboBox>
#include <QDialog>
#include <QString>
#include <QSyntaxHighlighter>

#include "Common/Flag.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

class ControlReference;
class MappingWidget;
class QAbstractButton;
class QDialogButtonBox;
class QLineEdit;
class QTableWidget;
class QVBoxLayout;
class QWidget;
class QPlainTextEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QLabel;

namespace ControllerEmu
{
class EmulatedController;
class NumericSettingBase;
}  // namespace ControllerEmu

class InputStateLineEdit;

class ControlExpressionSyntaxHighlighter final : public QSyntaxHighlighter
{
  Q_OBJECT
public:
  explicit ControlExpressionSyntaxHighlighter(QTextDocument* parent);

protected:
  void highlightBlock(const QString& text) final override;
};

class QComboBoxWithMouseWheelDisabled : public QComboBox
{
  Q_OBJECT
public:
  explicit QComboBoxWithMouseWheelDisabled(QWidget* parent = nullptr) : QComboBox(parent) {}

protected:
  // Consumes event while doing nothing (the combo box will still scroll)
  void wheelEvent(QWheelEvent* event) override;
};

class IOWindow final : public QDialog
{
  Q_OBJECT
public:
  enum class Type
  {
    Input,
    InputSetting,
    Output
  };

  explicit IOWindow(MappingWidget* parent, ControllerEmu::EmulatedController* controller,
                    ControlReference* ref, Type type, const QString& name,
                    ControllerEmu::NumericSettingBase* numeric_setting = nullptr);

private:
  std::shared_ptr<ciface::Core::Device> GetSelectedDevice() const;

  void CreateMainLayout();
  void AddFunction(std::string function_name);
  void ConnectWidgets();
  void ConfigChanged();
  void Update();

  void OnDialogButtonPressed(QAbstractButton* button);
  void OnDeviceChanged();
  void OnDetectButtonPressed();
  void OnTestSelectedButtonPressed();
  void OnTestResultsButtonPressed();
  void OnUIRangeChanged(int range);
  void OnRangeChanged();

  void AppendSelectedOption();
  void UpdateOptionList();
  void UpdateDeviceList();
  void ReleaseDevices();

  enum class UpdateMode
  {
    Normal,
    Force,
  };

  void UpdateExpression(std::string new_expression, UpdateMode mode = UpdateMode::Normal);

  ControlState GetNumericSettingValue() const;

  // Main Layout
  QVBoxLayout* m_main_layout;

  // Devices
  QComboBox* m_devices_combo;

  // Options
  QTableWidget* m_option_list;

  // Range
  QSlider* m_range_slider;
  QSpinBox* m_range_spinbox;

  // Shared actions
  QPushButton* m_help_button;
  QPushButton* m_select_button;
  QComboBox* m_operators_combo;
  QComboBox* m_functions_combo;
  QComboBox* m_variables_combo;

  // Input actions
  QPushButton* m_detect_button;

  // Output actions
  QPushButton* m_test_selected_button;
  QPushButton* m_test_results_button;

  // TextArea
  QPlainTextEdit* m_expression_text;
  InputStateLineEdit* m_parse_text;
  QLabel* m_focus_label;

  // ButtonBox
  QDialogButtonBox* m_button_box;
  QPushButton* m_clear_button;

  ControlReference* m_reference;
  std::string m_original_expression;
  ControlState m_original_range;
  ControllerEmu::EmulatedController* m_controller;

  ciface::Core::DeviceQualifier m_devq;
  Type m_type;
  ControllerEmu::NumericSettingBase* m_numeric_setting;
  std::shared_ptr<ciface::Core::Device> m_selected_device;
  std::mutex m_selected_device_mutex;
  std::vector<QString> m_functions_parameters;
};
