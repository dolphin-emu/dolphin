// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

namespace AudioCommon
{
enum class DPL2Quality;
}

class ConfigBool;
class ConfigChoice;
class ConfigComplexChoice;
class ConfigRadioBool;
class ConfigSlider;
class ConfigStringChoice;
class QHBoxLayout;
class QLabel;
class QRadioButton;
class SettingsWindow;
class ConfigBool;

class AudioPane final : public QWidget
{
  Q_OBJECT
public:
  explicit AudioPane();

private:
  void CreateWidgets();
  void ConnectWidgets();
  void AddDescriptions();

  void OnEmulationStateChanged(bool running);
  void OnBackendChanged();
  void OnDspChanged();

  void CheckNeedForLatencyControl();
  bool m_latency_control_supported;

  QHBoxLayout* m_main_layout;

  // DSP Engine
  ConfigComplexChoice* m_dsp_combo;

  // Volume
  ConfigSlider* m_volume_slider;
  QLabel* m_volume_indicator;

  // Backend
  QLabel* m_backend_label;
  ConfigStringChoice* m_backend_combo;

  ConfigBool* m_dolby_pro_logic;
  QLabel* m_dolby_quality_label;
  ConfigChoice* m_dolby_quality_combo;

  QLabel* m_latency_label;
  ConfigSlider* m_latency_slider;
#ifdef _WIN32
  QLabel* m_wasapi_device_label;
  ConfigStringChoice* m_wasapi_device_combo;
#endif

  // Misc Settings
  ConfigBool* m_audio_fill_gaps;
  ConfigBool* m_audio_preserve_pitch;
  ConfigBool* m_speed_up_mute_enable;
};
