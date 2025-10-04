// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QPushButton>
#include <QWidget>

#include "Core/PowerPC/PowerPC.h"

class ConfigBool;
template <typename T>
class ConfigChoiceMap;
class ConfigFloatSlider;
class ConfigSlider;
class ConfigSliderU32;
class QCheckBox;
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

  void OnResetButtonClicked();

  ConfigChoiceMap<PowerPC::CPUCore>* m_cpu_emulation_engine_combobox;
  ConfigBool* m_enable_mmu_checkbox;
  ConfigBool* m_pause_on_panic_checkbox;
  ConfigBool* m_accurate_cpu_cache_checkbox;
  ConfigBool* m_cpu_clock_override_checkbox;
  ConfigFloatSlider* m_cpu_clock_override_slider;
  QLabel* m_cpu_label;

  ConfigBool* m_vi_rate_override_checkbox;
  ConfigFloatSlider* m_vi_rate_override_slider;
  QLabel* m_vi_label;

  ConfigBool* m_custom_rtc_checkbox;
  QDateTimeEdit* m_custom_rtc_datetime;

  ConfigBool* m_ram_override_checkbox;
  ConfigSliderU32* m_mem1_override_slider;
  QLabel* m_mem1_label;
  ConfigSliderU32* m_mem2_override_slider;
  QLabel* m_mem2_label;

  QPushButton* m_reset_button;
};
