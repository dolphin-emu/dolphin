// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QComboBox>
#include <QWidget>

namespace AudioCommon
{
enum class DPL2Quality;
}

class QCheckBox;
class QLabel;
class QGridLayout;
class QRadioButton;
class QSlider;
class QSpinBox;
class SettingsWindow;

class ClickEventComboBox : public QComboBox
{
  Q_OBJECT
public:
  class AudioPane* m_audio_pane = nullptr;

private:
  virtual void showPopup() override;
};

class AudioPane final : public QWidget
{
  Q_OBJECT
public:
  explicit AudioPane();

  void OnCustomShowPopup(QWidget* widget);

protected:
  virtual void showEvent(QShowEvent* event) override;
  virtual void hideEvent(QHideEvent* event) override;

private:
  void CreateWidgets();
  void ConnectWidgets();

  void LoadSettings();
  void SaveSettings();

  void OnEmulationStateChanged(bool running);
  void OnBackendChanged();
#ifdef _WIN32
  void OnWASAPIDeviceChanged();
  void LoadWASAPIDevice();
  void LoadWASAPIDeviceSampleRate();
  std::string GetWASAPIDeviceSampleRate() const;
#endif
  void OnDSPChanged();
  void OnVolumeChanged(int volume);

  void CheckNeedForLatencyControl();
  bool m_latency_control_supported;

  QString GetDPL2QualityAndLatencyLabel(AudioCommon::DPL2Quality value) const;
  void RefreshDolbyWidgets();
  void EnableDolbyQualityWidgets(bool enabled) const;

  bool m_running;
  bool m_ignore_save_settings;
  QTimer* m_timer;

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
  QLabel* m_dolby_quality_latency_label;
  QLabel* m_latency_label;
  QSpinBox* m_latency_spin;
  QCheckBox* m_use_os_sample_rate;
#ifdef _WIN32
  QLabel* m_wasapi_device_label;
  QLabel* m_wasapi_device_sample_rate_label;
  ClickEventComboBox* m_wasapi_device_combo;
  QComboBox* m_wasapi_device_sample_rate_combo;
  bool m_wasapi_device_supports_default_sample_rate;
#endif

  // Audio Stretching
  QCheckBox* m_stretching_enable;
  QLabel* m_emu_speed_tolerance_label;
  QSlider* m_emu_speed_tolerance_slider;
  QLabel* m_emu_speed_tolerance_indicator;
};
