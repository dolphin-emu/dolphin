// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class ConfigBool;
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
  ConfigBool* m_enable_mmu_checkbox;
  ConfigBool* m_pause_on_panic_checkbox;
  ConfigBool* m_accurate_cpu_cache_checkbox;
  QCheckBox* m_cpu_clock_override_checkbox;
  QSlider* m_cpu_clock_override_slider;
  QLabel* m_cpu_clock_override_slider_label;
  QLabel* m_cpu_clock_override_description;

  QCheckBox* m_custom_rtc_checkbox;
  QDateTimeEdit* m_custom_rtc_datetime;

  QCheckBox* m_ram_override_checkbox;
  QSlider* m_mem1_override_slider;
  QLabel* m_mem1_override_slider_label;
  QSlider* m_mem2_override_slider;
  QLabel* m_mem2_override_slider_label;
  QLabel* m_ram_override_description;
};
