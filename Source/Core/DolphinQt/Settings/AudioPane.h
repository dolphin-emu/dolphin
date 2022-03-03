// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

namespace AudioCommon
{
enum class DPL2Quality;
}

class QCheckBox;
class QComboBox;
class QHBoxLayout;
class QLabel;
class QRadioButton;
class QSlider;
class QSpinBox;
class SettingsWindow;

class AudioPane final : public QWidget
{
  Q_OBJECT
public:
  explicit AudioPane();

private:
  void CreateWidgets();
  void ConnectWidgets();

  void LoadSettings();
  void SaveSettings();

  void OnEmulationStateChanged(bool running);
  void OnBackendChanged();
  void OnDspChanged();
  void OnVolumeChanged(int volume);

  void CheckNeedForLatencyControl();
  bool m_latency_control_supported;

  QString GetDPL2QualityLabel(AudioCommon::DPL2Quality value) const;
  QString GetDPL2ApproximateLatencyLabel(AudioCommon::DPL2Quality value) const;
  void EnableDolbyQualityWidgets(bool enabled) const;

  QHBoxLayout* m_main_layout;

  // DSP Engine
  QRadioButton* m_dsp_hle;
  QRadioButton* m_dsp_lle;
  QRadioButton* m_dsp_interpreter;

  // Volume
  QSlider* m_volume_slider;
  QLabel* m_volume_indicator;

  // Backend
  QLabel* m_backend_label;
  QComboBox* m_backend_combo;
  QCheckBox* m_dolby_pro_logic;
  QLabel* m_dolby_quality_label;
  QSlider* m_dolby_quality_slider;
  QLabel* m_dolby_quality_low_label;
  QLabel* m_dolby_quality_highest_label;
  QLabel* m_dolby_quality_latency_label;
  QLabel* m_latency_label;
  QSpinBox* m_latency_spin;
#ifdef _WIN32
  QLabel* m_wasapi_device_label;
  QComboBox* m_wasapi_device_combo;
#endif

  // Audio Stretching
  QCheckBox* m_stretching_enable;
  QLabel* m_stretching_buffer_label;
  QSlider* m_stretching_buffer_slider;
  QLabel* m_stretching_buffer_indicator;
};
