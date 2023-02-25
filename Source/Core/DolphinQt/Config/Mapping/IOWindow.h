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

namespace ControllerEmu
{
class EmulatedController;
}

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
  // Consumes event while doing nothing
  void wheelEvent(QWheelEvent* event) override;
};

class IOWindow final : public QDialog
{
  Q_OBJECT
public:
  enum class Type
  {
    Input,
    Output
  };

  explicit IOWindow(MappingWidget* parent, ControllerEmu::EmulatedController* m_controller,
                    ControlReference* ref, Type type);

private:
  std::shared_ptr<ciface::Core::Device> GetSelectedDevice() const;

  void CreateMainLayout();
  void ConnectWidgets();
  void ConfigChanged();
  void Update();

  void OnDialogButtonPressed(QAbstractButton* button);
  void OnDeviceChanged();
  void OnDetectButtonPressed();
  void OnTestButtonPressed();
  void OnRangeChanged(int range);

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

  // Main Layout
  QVBoxLayout* m_main_layout;

  // Devices
  QComboBox* m_devices_combo;

  // Options
  QTableWidget* m_option_list;

  // Scalar
  QSpinBox* m_scalar_spinbox;

  // Shared actions
  QPushButton* m_select_button;
  QComboBox* m_operators_combo;
  QComboBox* m_variables_combo;

  // Input actions
  QPushButton* m_detect_button;
  QComboBox* m_functions_combo;

  // Output actions
  QPushButton* m_test_button;

  // Textarea
  QPlainTextEdit* m_expression_text;
  InputStateLineEdit* m_parse_text;

  // Buttonbox
  QDialogButtonBox* m_button_box;
  QPushButton* m_clear_button;

  ControlReference* m_reference;
  std::string m_original_expression;
  ControllerEmu::EmulatedController* m_controller;

  ciface::Core::DeviceQualifier m_devq;
  Type m_type;
  std::shared_ptr<ciface::Core::Device> m_selected_device;
  std::mutex m_selected_device_mutex;
};
