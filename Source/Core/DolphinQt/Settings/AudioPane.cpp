// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Settings/AudioPane.h"

#include <cassert>

#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QSlider>
#include <QSpacerItem>
#include <QSpinBox>
#include <QVBoxLayout>

#include "AudioCommon/AudioCommon.h"
#include "AudioCommon/Enums.h"
#include "AudioCommon/WASAPIStream.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "DolphinQt/Config/SettingsWindow.h"
#include "DolphinQt/Settings.h"

AudioPane::AudioPane()
{
  m_running = false;
  m_ignore_save_settings = false;

  CheckNeedForLatencyControl();
  CreateWidgets();
  LoadSettings();
  ConnectWidgets();

  connect(&Settings::Instance(), &Settings::VolumeChanged, this, &AudioPane::OnVolumeChanged);
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          [=](Core::State state) { OnEmulationStateChanged(state != Core::State::Uninitialized); });

  OnEmulationStateChanged(Core::GetState() != Core::State::Uninitialized);
}

void AudioPane::CreateWidgets()
{
  auto* dsp_box = new QGroupBox(tr("DSP Emulation Engine"));
  auto* dsp_layout = new QVBoxLayout;

  dsp_box->setLayout(dsp_layout);
  m_dsp_hle = new QRadioButton(tr("DSP HLE (fast)"));
  m_dsp_lle = new QRadioButton(tr("DSP LLE Recompiler"));
  m_dsp_interpreter = new QRadioButton(tr("DSP LLE Interpreter (slow)"));

  dsp_layout->addStretch(1);
  dsp_layout->addWidget(m_dsp_hle);
  dsp_layout->addWidget(m_dsp_lle);
  dsp_layout->addWidget(m_dsp_interpreter);
  dsp_layout->addStretch(1);

  auto* volume_box = new QGroupBox(tr("Volume"));
  auto* volume_layout = new QVBoxLayout;
  m_volume_slider = new QSlider;
  m_volume_indicator = new QLabel();

  volume_box->setLayout(volume_layout);

  m_volume_slider->setMinimum(0);
  m_volume_slider->setMaximum(100);

  m_volume_slider->setToolTip(tr("Using this is preferred over the OS mixer volume"));

  m_volume_indicator->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
  m_volume_indicator->setFixedWidth(QFontMetrics(font()).boundingRect(tr("%1 %").arg(100)).width());

  volume_layout->addWidget(m_volume_slider, 0, Qt::AlignHCenter);
  volume_layout->addWidget(m_volume_indicator, 0, Qt::AlignHCenter);

  auto* backend_box = new QGroupBox(tr("Backend Settings"));
  auto* backend_layout = new QFormLayout;
  backend_box->setLayout(backend_layout);
  m_backend_label = new QLabel(tr("Audio Backend:"));
  m_backend_combo = new QComboBox();
  m_dolby_pro_logic = new QCheckBox(tr("Dolby Pro Logic II Decoder"));

  if (m_latency_control_supported)
  {
    m_latency_label = new QLabel(tr("Latency:"));
    m_latency_spin = new QSpinBox();
    m_latency_spin->setMinimum(0);
    m_latency_spin->setMaximum(std::min((int)AudioCommon::GetMaxSupportedLatency(), 200));
    m_latency_spin->setToolTip(tr("Target latency (in ms). Higher values may reduce audio"
                                  " crackling.\nCertain backends only. Values above 20ms are not suggested."));
  }

  m_use_os_sample_rate = new QCheckBox(tr("Use OS Mixer sample rate"));

  m_use_os_sample_rate->setToolTip(
      tr("Directly mixes at the current OS mixer sample rate as opposed to %1"
         "Hz.\nThis will make resampling from 32kHz sources more accurate, possibly improving "
         "audio\n"
         "quality at the cost of performance. It won't follow changes to your OS setting."
         "\nIf unsure, leave off.")
          .arg(AudioCommon::GetDefaultSampleRate()));

  // Unfortunately this creates an empty space if added to the widget. We should find a better way
  m_use_os_sample_rate->setHidden(true);

  m_dolby_pro_logic->setToolTip(
      tr("Enables Dolby Pro Logic II emulation using 5.1 surround.\nCertain backends and DPS "
         "emulation engines only.\nAutomatically disabled if not supported by your audio device."
         "\nYou need to enable it in the game settings for GC games or in the menu settings on Wii."
         "\nThe console will still output 2.0, but the encoder will extract information for 5.1."
         "\nIf unsure, leave off."));

  auto* dolby_quality_layout = new QHBoxLayout;

  m_dolby_quality_label = new QLabel(tr("Decoding Quality:"));

  m_dolby_quality_slider = new QSlider(Qt::Horizontal);
  m_dolby_quality_slider->setMinimum(0);
  m_dolby_quality_slider->setMaximum(3);
  m_dolby_quality_slider->setPageStep(1);
  m_dolby_quality_slider->setTickPosition(QSlider::TicksBelow);
  m_dolby_quality_slider->setToolTip(
      tr("Quality of the DPLII decoder. Also increases audio latency"));
  m_dolby_quality_slider->setTracking(true);

  m_dolby_quality_low_label = new QLabel(GetDPL2QualityLabel(AudioCommon::DPL2Quality::Lowest));
  m_dolby_quality_highest_label =
      new QLabel(GetDPL2QualityLabel(AudioCommon::DPL2Quality::Highest));
  m_dolby_quality_latency_label =
      new QLabel(GetDPL2ApproximateLatencyLabel(AudioCommon::DPL2Quality::Highest));

  dolby_quality_layout->addWidget(m_dolby_quality_low_label);
  dolby_quality_layout->addWidget(m_dolby_quality_slider);
  dolby_quality_layout->addWidget(m_dolby_quality_highest_label);

  backend_layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  backend_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  backend_layout->addRow(m_backend_label, m_backend_combo);
  if (m_latency_control_supported)
    backend_layout->addRow(m_latency_label, m_latency_spin);

#ifdef _WIN32
  m_wasapi_device_label = new QLabel(tr("Device:"));
  m_wasapi_device_sample_rate_label = new QLabel(tr("Device Sample Rate:"));
  m_wasapi_device_combo = new QComboBox;
  m_wasapi_device_sample_rate_combo = new QComboBox;

  m_wasapi_device_combo->setToolTip(tr("Some devices might not work with WASAPI Exclusive mode"));
  m_wasapi_device_sample_rate_combo->setToolTip(
      tr("Output (and mix) sample rate.\nAnything above 48kHz will have very minimal "
         "improvements to quality at the cost of performance."));

  backend_layout->addRow(m_wasapi_device_label, m_wasapi_device_combo);
  backend_layout->addRow(m_wasapi_device_sample_rate_label, m_wasapi_device_sample_rate_combo);
#endif
  backend_layout->addWidget(m_use_os_sample_rate);

  backend_layout->addRow(m_dolby_pro_logic);
  backend_layout->addRow(m_dolby_quality_label);
  backend_layout->addRow(dolby_quality_layout);
  backend_layout->addRow(m_dolby_quality_latency_label);

  auto* mixer_box = new QGroupBox(tr("Mixer Settings"));
  auto* mixer_layout = new QGridLayout;
  m_stretching_enable = new QCheckBox(tr("Audio Stretching"));
  m_emu_speed_tolerance_slider = new QSlider(Qt::Horizontal);
  m_emu_speed_tolerance_indicator = new QLabel();
  m_emu_speed_tolerance_indicator->setAlignment(Qt::AlignRight);
  m_emu_speed_tolerance_label = new QLabel(tr("Emulation Speed Tolerance:"));
  mixer_box->setLayout(mixer_layout);

  m_stretching_enable->setToolTip(
      tr("Enables stretching of the audio (pitch correction) to match the emulation speed"));

  m_emu_speed_tolerance_slider->setMinimum(-1);
  m_emu_speed_tolerance_slider->setMaximum(100);
  m_emu_speed_tolerance_slider->setToolTip(
      tr("Time(ms) we need to fall behind the emulation for sound to start using the actual "
         "emulation speed.\nIf set "
         "too high (>40), sound will lose quality when we slow down or stutter.\nIf set too low (<10), "
         "sound might "
         "lose quality if you have frequent small stutters.\nSet 0 to "
         "have it on all the times. Slide all the way left to disable.")); //To find best default (and review description at 0)
  
  mixer_layout->addWidget(m_stretching_enable, 0, 0, 1, -1);
  mixer_layout->addWidget(m_emu_speed_tolerance_label, 1, 0);
  mixer_layout->addWidget(m_emu_speed_tolerance_slider, 1, 1);
  mixer_layout->addWidget(m_emu_speed_tolerance_indicator, 1, 2);

  m_main_layout = new QGridLayout;

  m_main_layout->setRowStretch(0, 0);

  dsp_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  m_main_layout->addWidget(dsp_box, 0, 0);
  m_main_layout->addWidget(volume_box, 0, 1, -1, 1);
  m_main_layout->addWidget(backend_box, 1, 0);
  m_main_layout->addWidget(mixer_box, 2, 0);

  setLayout(m_main_layout);
}

void AudioPane::ConnectWidgets()
{
  connect(m_backend_combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &AudioPane::SaveSettings);
  connect(m_volume_slider, &QSlider::valueChanged, this, &AudioPane::SaveSettings);
  if (m_latency_control_supported)
  {
    connect(m_latency_spin, qOverload<int>(&QSpinBox::valueChanged), this,
            &AudioPane::SaveSettings);
  }
  connect(m_emu_speed_tolerance_slider, &QSlider::valueChanged, this, &AudioPane::SaveSettings);
  connect(m_use_os_sample_rate, &QCheckBox::toggled, this, &AudioPane::SaveSettings);
  connect(m_dolby_pro_logic, &QCheckBox::toggled, this, &AudioPane::SaveSettings);
  connect(m_dolby_quality_slider, &QSlider::valueChanged, this, &AudioPane::SaveSettings);
  connect(m_stretching_enable, &QCheckBox::toggled, this, &AudioPane::SaveSettings);
  connect(m_dsp_hle, &QRadioButton::toggled, this, &AudioPane::SaveSettings);
  connect(m_dsp_lle, &QRadioButton::toggled, this, &AudioPane::SaveSettings);
  connect(m_dsp_interpreter, &QRadioButton::toggled, this, &AudioPane::SaveSettings);

#ifdef _WIN32
  connect(m_wasapi_device_combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &AudioPane::SaveSettings);
  connect(m_wasapi_device_sample_rate_combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &AudioPane::SaveSettings);
#endif
}

void AudioPane::LoadSettings()
{
  auto& settings = Settings::Instance();

  // DSP
  if (SConfig::GetInstance().bDSPHLE)
  {
    m_dsp_hle->setChecked(true);
  }
  else
  {
    m_dsp_lle->setChecked(SConfig::GetInstance().m_DSPEnableJIT);
    m_dsp_interpreter->setChecked(!SConfig::GetInstance().m_DSPEnableJIT);
  }

  // Backend
  m_ignore_save_settings = true;
  const auto current = SConfig::GetInstance().sBackend;
  bool selection_set = false;
  m_backend_combo->clear();
  for (const auto& backend : AudioCommon::GetSoundBackends())
  {
    m_backend_combo->addItem(tr(backend.c_str()), QVariant(QString::fromStdString(backend)));
    if (backend == current)
    {
      m_backend_combo->setCurrentIndex(m_backend_combo->count() - 1);
      selection_set = true;
    }
  }
  if (!selection_set)
    m_backend_combo->setCurrentIndex(-1);
  m_ignore_save_settings = false;

  OnBackendChanged();

  // Volume
  OnVolumeChanged(settings.GetVolume());

  // DPL2
  m_dolby_pro_logic->setChecked(SConfig::GetInstance().bDPL2Decoder);
  m_ignore_save_settings = true;
  m_dolby_quality_slider->setValue(int(Config::Get(Config::MAIN_DPL2_QUALITY)));
  m_ignore_save_settings = false;
  m_dolby_quality_latency_label->setText(
      GetDPL2ApproximateLatencyLabel(Config::Get(Config::MAIN_DPL2_QUALITY)));
  if (AudioCommon::SupportsDPL2Decoder(current) && !m_dsp_hle->isChecked())
  {
    EnableDolbyQualityWidgets(m_dolby_pro_logic->isEnabled() && m_dolby_pro_logic->isChecked());
  }

  // Latency
  if (m_latency_control_supported)
  {
    m_ignore_save_settings = true;
    m_latency_spin->setValue(SConfig::GetInstance().iLatency);
    m_ignore_save_settings = false;
  }

  m_ignore_save_settings = true;

  // Sample rate
  m_use_os_sample_rate->setChecked(SConfig::GetInstance().bUseOSMixerSampleRate);

  // Stretch
  m_stretching_enable->setChecked(SConfig::GetInstance().m_audio_stretch);
  m_emu_speed_tolerance_slider->setValue(SConfig::GetInstance().m_audio_emu_speed_tolerance);
  if (m_emu_speed_tolerance_slider->value() < 0)
    m_emu_speed_tolerance_indicator->setText(tr("Disabled"));
  else
    m_emu_speed_tolerance_indicator->setText(
        tr("%1 ms").arg(m_emu_speed_tolerance_slider->value()));

  m_ignore_save_settings = false;

#ifdef _WIN32
  m_ignore_save_settings = true;
  if (SConfig::GetInstance().sWASAPIDevice == "default")
  {
    m_wasapi_device_combo->setCurrentIndex(0);
  }
  else
  {
    m_wasapi_device_combo->setCurrentText(
        QString::fromStdString(SConfig::GetInstance().sWASAPIDevice));
    // Saved device not found, reset it
    if (m_wasapi_device_combo->currentIndex() < 1)
    {
      SConfig::GetInstance().sWASAPIDevice = "default";
    }
  }
  LoadWASAPIDeviceSampleRate();
  m_ignore_save_settings = false;
#endif

  // Call this again to "clamp" values that might not have been accepted
  SaveSettings();
}

void AudioPane::SaveSettings()
{
  // Avoids multiple calls to this when we are modifying the widgets
  //  in a way that would trigger multiple SaveSettings() callbacks
  if (m_ignore_save_settings)
  {
    return;
  }

  auto& settings = Settings::Instance();

  bool volume_changed = false;
  bool backend_setting_changed = false;
  bool surround_enabled_changed = false;

  // DSP
  if (SConfig::GetInstance().bDSPHLE != m_dsp_hle->isChecked() ||
      SConfig::GetInstance().m_DSPEnableJIT != m_dsp_lle->isChecked())
  {
    OnDspChanged();
  }
  SConfig::GetInstance().bDSPHLE = m_dsp_hle->isChecked();
  Config::SetBaseOrCurrent(Config::MAIN_DSP_HLE, m_dsp_hle->isChecked());
  SConfig::GetInstance().m_DSPEnableJIT = m_dsp_lle->isChecked();
  Config::SetBaseOrCurrent(Config::MAIN_DSP_JIT, m_dsp_lle->isChecked());

  // Backend
  const auto selection =
      m_backend_combo->itemData(m_backend_combo->currentIndex()).toString().toStdString();
  auto& backend = SConfig::GetInstance().sBackend;

  if (selection != backend)
  {
    backend = selection;
    OnBackendChanged();
  }

  // Volume
  if (m_volume_slider->value() != settings.GetVolume())
  {
    settings.SetVolume(m_volume_slider->value());
    OnVolumeChanged(settings.GetVolume());

    volume_changed = true;
  }

  // DPL2
  if (SConfig::GetInstance().bDPL2Decoder != m_dolby_pro_logic->isChecked())
  {
    SConfig::GetInstance().bDPL2Decoder = m_dolby_pro_logic->isChecked();
    backend_setting_changed = true;
    surround_enabled_changed = true;
  }
  if (Config::Get(Config::MAIN_DPL2_QUALITY) !=
      static_cast<AudioCommon::DPL2Quality>(m_dolby_quality_slider->value()))
  {
    Config::SetBase(Config::MAIN_DPL2_QUALITY,
                    static_cast<AudioCommon::DPL2Quality>(m_dolby_quality_slider->value()));
    m_dolby_quality_latency_label->setText(
        GetDPL2ApproximateLatencyLabel(Config::Get(Config::MAIN_DPL2_QUALITY)));
    backend_setting_changed = true; // Not a mistake
  }
  if (AudioCommon::SupportsDPL2Decoder(backend) && !m_dsp_hle->isChecked())
  {
    EnableDolbyQualityWidgets(m_dolby_pro_logic->isEnabled() && m_dolby_pro_logic->isChecked());
  }

  // Latency
  if (m_latency_control_supported && SConfig::GetInstance().iLatency != m_latency_spin->value())
  {
    SConfig::GetInstance().iLatency = m_latency_spin->value();
    backend_setting_changed = true;
  }

  // Sample rate
  if (m_use_os_sample_rate->isChecked() != SConfig::GetInstance().bUseOSMixerSampleRate)
  {
    SConfig::GetInstance().bUseOSMixerSampleRate = m_use_os_sample_rate->isChecked();
    backend_setting_changed = true;
  }

  // Stretch
  SConfig::GetInstance().m_audio_stretch = m_stretching_enable->isChecked();
  SConfig::GetInstance().m_audio_emu_speed_tolerance = m_emu_speed_tolerance_slider->value();
  if (m_emu_speed_tolerance_slider->value() < 0)
    m_emu_speed_tolerance_indicator->setText(tr("Disabled"));
  else
    m_emu_speed_tolerance_indicator->setText(
        tr("%1 ms").arg(m_emu_speed_tolerance_slider->value()));

#ifdef _WIN32
  // If left at default, Dolphin will automatically
  // pick a device and sample rate
  std::string device = "default";

  if (m_wasapi_device_combo->currentIndex() > 0)
    device = m_wasapi_device_combo->currentText().toStdString();

  if (SConfig::GetInstance().sWASAPIDevice != device)
  {
    assert(device != "");
    SConfig::GetInstance().sWASAPIDevice = device;

    bool is_wasapi = backend == BACKEND_WASAPI;
    if (is_wasapi)
    {
      OnWASAPIDeviceChanged();
      LoadWASAPIDeviceSampleRate();
      backend_setting_changed = true;
    }
  }

  std::string deviceSampleRate = "0";

  bool canSelectDeviceSampleRate = SConfig::GetInstance().sWASAPIDevice != "default";
  if (m_wasapi_device_sample_rate_combo->currentIndex() > 0 && canSelectDeviceSampleRate)
  {
    QString qs = m_wasapi_device_sample_rate_combo->currentText();
    qs.chop(2);  // Remove "Hz" from the end
    deviceSampleRate = qs.toStdString();
  }

  if (SConfig::GetInstance().sWASAPIDeviceSampleRate != deviceSampleRate)
  {
    SConfig::GetInstance().sWASAPIDeviceSampleRate = deviceSampleRate;
    backend_setting_changed = true;
  }
#endif

  AudioCommon::UpdateSoundStreamSettings(volume_changed, backend_setting_changed,
                                         surround_enabled_changed);
}

void AudioPane::OnDspChanged()
{
  const auto backend = SConfig::GetInstance().sBackend;

  m_dolby_pro_logic->setEnabled(AudioCommon::SupportsDPL2Decoder(backend) &&
                                !m_dsp_hle->isChecked());
  EnableDolbyQualityWidgets(m_dolby_pro_logic->isEnabled() && m_dolby_pro_logic->isChecked());
}

void AudioPane::OnBackendChanged()
{
  const auto backend = SConfig::GetInstance().sBackend;

  m_use_os_sample_rate->setEnabled(backend != BACKEND_NULLSOUND);

  m_dolby_pro_logic->setEnabled(AudioCommon::SupportsDPL2Decoder(backend) &&
                                !m_dsp_hle->isChecked());
  EnableDolbyQualityWidgets(m_dolby_pro_logic->isEnabled() && m_dolby_pro_logic->isChecked());

  if (m_latency_control_supported)
  {
    m_latency_label->setEnabled(AudioCommon::SupportsLatencyControl(backend));
    m_latency_spin->setEnabled(AudioCommon::SupportsLatencyControl(backend));
  }

#ifdef _WIN32
  bool is_wasapi = backend == BACKEND_WASAPI;
  m_wasapi_device_label->setHidden(!is_wasapi);
  m_wasapi_device_sample_rate_label->setHidden(!is_wasapi);
  m_wasapi_device_combo->setHidden(!is_wasapi);
  m_wasapi_device_sample_rate_combo->setHidden(!is_wasapi);

  // TODO: implement this in other Operative Systems,
  // as of now this is only supported on (all) Windows backends
  m_use_os_sample_rate->setHidden(is_wasapi);

  if (is_wasapi)
  {
    m_ignore_save_settings = true;

    m_wasapi_device_combo->clear();
    m_wasapi_device_combo->addItem(tr("Default Device"));

    for (const auto device : WASAPIStream::GetAvailableDevices())
      m_wasapi_device_combo->addItem(QString::fromStdString(device));

    OnWASAPIDeviceChanged();

    m_ignore_save_settings = false;
  }
#endif

  m_volume_slider->setEnabled(AudioCommon::SupportsVolumeChanges(backend));
  m_volume_indicator->setEnabled(AudioCommon::SupportsVolumeChanges(backend));
}

#ifdef _WIN32
void AudioPane::OnWASAPIDeviceChanged()
{
  m_ignore_save_settings = true;

  m_wasapi_device_sample_rate_combo->clear();
  // Don't allow users to select a sample rate for the default device,
  // even though that would be possible, the default device can change
  // at any time so it wouldn't make sense
  bool canSelectDeviceSampleRate = SConfig::GetInstance().sWASAPIDevice != "default";
  if (canSelectDeviceSampleRate)
  {
    m_wasapi_device_sample_rate_combo->setEnabled(true);
    m_wasapi_device_sample_rate_label->setEnabled(true);

    m_wasapi_device_sample_rate_combo->addItem(
        tr("Default Dolphin Sample Rate (%1Hz)").arg(AudioCommon::GetDefaultSampleRate()));

    for (const auto deviceSampleRate : WASAPIStream::GetSelectedDeviceSampleRates())
    {
      m_wasapi_device_sample_rate_combo->addItem(
          QString::number(deviceSampleRate).append(tr("Hz")));
    }
  }
  else
  {
    m_wasapi_device_sample_rate_combo->setEnabled(false);
    m_wasapi_device_sample_rate_label->setEnabled(false);
    if (m_running)
    {
      std::string sample_rate_text;
      sample_rate_text = SConfig::GetInstance().sWASAPIDeviceSampleRate + "Hz";
      if (sample_rate_text == "0Hz")
      {
        sample_rate_text = std::to_string(AudioCommon::GetDefaultSampleRate()) + "Hz";
      }
      m_wasapi_device_sample_rate_combo->addItem(tr(sample_rate_text.c_str()));
    }
    else
    {
      m_wasapi_device_sample_rate_combo->addItem(
          tr("Select a Device (%1Hz)").arg(AudioCommon::GetDefaultSampleRate()));
    }
  }

  m_ignore_save_settings = false;
}

void AudioPane::LoadWASAPIDeviceSampleRate()
{
  bool canSelectDeviceSampleRate = SConfig::GetInstance().sWASAPIDevice != "default";
  if (SConfig::GetInstance().sWASAPIDeviceSampleRate == "0" || !canSelectDeviceSampleRate)
  {
    m_wasapi_device_sample_rate_combo->setCurrentIndex(0);
    SConfig::GetInstance().sWASAPIDeviceSampleRate = "0";
  }
  else
  {
    std::string sample_rate_text = SConfig::GetInstance().sWASAPIDeviceSampleRate + "Hz";
    m_wasapi_device_sample_rate_combo->setCurrentText(QString::fromStdString(sample_rate_text));
    // Saved sample rate not found, reset it
    if (m_wasapi_device_combo->currentIndex() < 1)
    {
      SConfig::GetInstance().sWASAPIDeviceSampleRate = "0";
    }
  }
}
#endif

void AudioPane::OnEmulationStateChanged(bool running)
{
  m_running = running;

  const auto backend = SConfig::GetInstance().sBackend;

  bool supports_current_emulation_state =
      !running || AudioCommon::BackendSupportsRuntimeSettingsChanges();

  m_dsp_hle->setEnabled(!running);
  m_dsp_lle->setEnabled(!running);
  m_dsp_interpreter->setEnabled(!running);
  m_backend_label->setEnabled(!running);
  m_backend_combo->setEnabled(!running);

  m_use_os_sample_rate->setEnabled(supports_current_emulation_state &&
                                   backend != BACKEND_NULLSOUND);

  if (AudioCommon::SupportsDPL2Decoder(backend) && !m_dsp_hle->isChecked())
  {
    // TODO: disable this later if the audio device turned out unable to support surround
    bool enable_dolby_pro_logic = supports_current_emulation_state;
    m_dolby_pro_logic->setEnabled(enable_dolby_pro_logic);
    EnableDolbyQualityWidgets(m_dolby_pro_logic->isEnabled() && m_dolby_pro_logic->isChecked());
  }
  if (m_latency_control_supported &&
      AudioCommon::SupportsLatencyControl(SConfig::GetInstance().sBackend))
  {
    bool enable_latency =
        supports_current_emulation_state && AudioCommon::SupportsLatencyControl(backend);
    m_latency_label->setEnabled(enable_latency);
    m_latency_spin->setEnabled(enable_latency);
  }

#ifdef _WIN32
  m_wasapi_device_label->setEnabled(supports_current_emulation_state);
  m_wasapi_device_combo->setEnabled(supports_current_emulation_state);
  bool canSelectDeviceSampleRate =
      SConfig::GetInstance().sWASAPIDevice != "default" && supports_current_emulation_state;
  m_wasapi_device_sample_rate_label->setEnabled(canSelectDeviceSampleRate);
  m_wasapi_device_sample_rate_combo->setEnabled(canSelectDeviceSampleRate);
  bool is_wasapi = backend == BACKEND_WASAPI;
  if (is_wasapi)
  {
    OnWASAPIDeviceChanged();
    m_ignore_save_settings = true;
    LoadWASAPIDeviceSampleRate();
    m_ignore_save_settings = false;
  }
#endif
}

void AudioPane::OnVolumeChanged(int volume)
{
  m_ignore_save_settings = true;
  m_volume_slider->setValue(volume);
  m_ignore_save_settings = false;
  m_volume_indicator->setText(tr("%1 %").arg(volume));
}

void AudioPane::CheckNeedForLatencyControl()
{
  std::vector<std::string> backends = AudioCommon::GetSoundBackends();
  // Don't show latency related widgets if none of our backends support latency
  m_latency_control_supported =
      std::any_of(backends.cbegin(), backends.cend(), AudioCommon::SupportsLatencyControl);
}

QString AudioPane::GetDPL2QualityLabel(AudioCommon::DPL2Quality value) const
{
  switch (value)
  {
  case AudioCommon::DPL2Quality::Lowest:
    return tr("Lowest");
  case AudioCommon::DPL2Quality::Low:
    return tr("Low");
  case AudioCommon::DPL2Quality::Highest:
    return tr("Highest");
  case AudioCommon::DPL2Quality::High:
  default:
    return tr("High");
  }
}

QString AudioPane::GetDPL2ApproximateLatencyLabel(AudioCommon::DPL2Quality value) const
{
  switch (value)
  {
  case AudioCommon::DPL2Quality::Lowest:
    return tr("Latency: ~10ms");
  case AudioCommon::DPL2Quality::Low:
    return tr("Latency: ~20ms");
  case AudioCommon::DPL2Quality::Highest:
    return tr("Latency: ~80ms");
  case AudioCommon::DPL2Quality::High:
  default:
    return tr("Latency: ~40ms");
  }
}

void AudioPane::EnableDolbyQualityWidgets(bool enabled) const
{
  m_dolby_quality_label->setEnabled(enabled);
  m_dolby_quality_slider->setEnabled(enabled);
  m_dolby_quality_low_label->setEnabled(enabled);
  m_dolby_quality_highest_label->setEnabled(enabled);
  m_dolby_quality_latency_label->setEnabled(enabled);
}
