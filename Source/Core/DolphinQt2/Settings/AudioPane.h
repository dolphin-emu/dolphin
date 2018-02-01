// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QGridLayout;
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
  void OnVolumeChanged(int volume);

  void CheckNeedForLatencyControl();
  bool m_latency_control_supported;

  QGridLayout* m_main_layout;

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
  QLabel* m_latency_label;
  QSpinBox* m_latency_spin;

  // Audio Stretching
  QCheckBox* m_stretching_enable;
  QLabel* m_stretching_buffer_label;
  QSlider* m_stretching_buffer_slider;
  QLabel* m_stretching_buffer_indicator;
};
