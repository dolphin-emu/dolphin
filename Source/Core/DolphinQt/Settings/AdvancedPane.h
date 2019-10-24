// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QRadioButton;
class QSlider;
class QDateTimeEdit;

namespace Core
{
enum class State;
}

class AdvancedPane final : public QWidget
{
  Q_OBJECT
public:
  explicit AdvancedPane(QWidget* parent = nullptr);

private:
  void CreateLayout();
  void ConnectLayout();
  void Update();

  QComboBox* m_cpu_emulation_engine_combobox;
  QCheckBox* m_cpu_clock_override_checkbox;
  QSlider* m_cpu_clock_override_slider;
  QLabel* m_cpu_clock_override_slider_label;
  QLabel* m_cpu_clock_override_description;

  QCheckBox* m_custom_rtc_checkbox;
  QDateTimeEdit* m_custom_rtc_datetime;
};
