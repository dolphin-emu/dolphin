// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>
#include <QString>
#include <QSyntaxHighlighter>

#include "Common/Flag.h"
#include "InputCommon/ControllerInterface/Device.h"

class ControlReference;
class MappingWidget;
class QAbstractButton;
class QComboBox;
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

class ControlExpressionSyntaxHighlighter final : public QSyntaxHighlighter
{
  Q_OBJECT
public:
  ControlExpressionSyntaxHighlighter(QTextDocument* parent, QLineEdit* result);

protected:
  void highlightBlock(const QString& text) final override;

private:
  QLineEdit* const m_result_text;
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

  std::shared_ptr<ciface::Core::Device> GetSelectedDevice();

private:
  void CreateMainLayout();
  void ConnectWidgets();
  void ConfigChanged();
  void Update();

  void OnDialogButtonPressed(QAbstractButton* button);
  void OnDeviceChanged(const QString& device);
  void OnDetectButtonPressed();
  void OnTestButtonPressed();
  void OnRangeChanged(int range);

  void AppendSelectedOption();
  void UpdateOptionList();
  void UpdateDeviceList();

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
  QPushButton* m_select_button;
  QComboBox* m_operators_combo;

  // Input actions
  QPushButton* m_detect_button;
  QComboBox* m_functions_combo;

  // Output actions
  QPushButton* m_test_button;

  // Textarea
  QPlainTextEdit* m_expression_text;
  QLineEdit* m_parse_text;

  // Buttonbox
  QDialogButtonBox* m_button_box;
  QPushButton* m_clear_button;
  QPushButton* m_apply_button;

  ControlReference* m_reference;
  ControllerEmu::EmulatedController* m_controller;

  ciface::Core::DeviceQualifier m_devq;
  Type m_type;
  std::shared_ptr<ciface::Core::Device> m_selected_device;
};
