// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Settings/AudioPane.h"

#include <QCheckBox>
#include <QComboBox>
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
#include "AudioCommon/WASAPIStream.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "DolphinQt/Config/SettingsWindow.h"
#include "DolphinQt/Settings.h"

AudioPane::AudioPane()
{
  CheckNeedForLatencyControl();
  CreateWidgets();
  LoadSettings();
  ConnectWidgets();

  connect(&Settings::Instance(), &Settings::VolumeChanged, this, &AudioPane::OnVolumeChanged);
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          [=](Core::State state) { OnEmulationStateChanged(state != Core::State::Uninitialized); });
}

void AudioPane::CreateWidgets()
{
  auto* dsp_box = new QGroupBox(tr("DSP Emulation Engine"));
  auto* dsp_layout = new QVBoxLayout;

  dsp_box->setLayout(dsp_layout);
  m_dsp_hle = new QRadioButton(tr("DSP HLE Emulation (fast)"));
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
    m_latency_spin->setMaximum(200);
    m_latency_spin->setToolTip(tr("Sets the latency (in ms). Higher values may reduce audio "
                                  "crackling. Certain backends only."));
  }

  m_dolby_pro_logic->setToolTip(
      tr("Enables Dolby Pro Logic II emulation using 5.1 surround. Certain backends only."));

  backend_layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  backend_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  backend_layout->addRow(m_backend_label, m_backend_combo);
  if (m_latency_control_supported)
    backend_layout->addRow(m_latency_label, m_latency_spin);

#ifdef _WIN32
  m_wasapi_device_label = new QLabel(tr("Device:"));
  m_wasapi_device_combo = new QComboBox;

  backend_layout->addRow(m_wasapi_device_label, m_wasapi_device_combo);
#endif

  backend_layout->addRow(m_dolby_pro_logic);

  auto* stretching_box = new QGroupBox(tr("Audio Stretching Settings"));
  auto* stretching_layout = new QGridLayout;
  m_stretching_enable = new QCheckBox(tr("Enable Audio Stretching"));
  m_stretching_buffer_slider = new QSlider(Qt::Horizontal);
  m_stretching_buffer_indicator = new QLabel();
  m_stretching_buffer_label = new QLabel(tr("Buffer Size:"));
  stretching_box->setLayout(stretching_layout);

  m_stretching_buffer_slider->setMinimum(5);
  m_stretching_buffer_slider->setMaximum(300);

  m_stretching_enable->setToolTip(tr("Enables stretching of the audio to match emulation speed."));
  m_stretching_buffer_slider->setToolTip(tr("Size of stretch buffer in milliseconds. "
                                            "Values too low may cause audio crackling."));

  stretching_layout->addWidget(m_stretching_enable, 0, 0, 1, -1);
  stretching_layout->addWidget(m_stretching_buffer_label, 1, 0);
  stretching_layout->addWidget(m_stretching_buffer_slider, 1, 1);
  stretching_layout->addWidget(m_stretching_buffer_indicator, 1, 2);

  m_main_layout = new QGridLayout;

  m_main_layout->setRowStretch(0, 0);

  dsp_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  m_main_layout->addWidget(dsp_box, 0, 0);
  m_main_layout->addWidget(volume_box, 0, 1, -1, 1);
  m_main_layout->addWidget(backend_box, 1, 0);
  m_main_layout->addWidget(stretching_box, 2, 0);

  setLayout(m_main_layout);
}

void AudioPane::ConnectWidgets()
{
  connect(m_backend_combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &AudioPane::SaveSettings);
  connect(m_volume_slider, &QSlider::valueChanged, this, &AudioPane::SaveSettings);
  if (m_latency_control_supported)
  {
    connect(m_latency_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            &AudioPane::SaveSettings);
  }
  connect(m_stretching_buffer_slider, &QSlider::valueChanged, this, &AudioPane::SaveSettings);
  connect(m_dolby_pro_logic, &QCheckBox::toggled, this, &AudioPane::SaveSettings);
  connect(m_stretching_enable, &QCheckBox::toggled, this, &AudioPane::SaveSettings);
  connect(m_dsp_hle, &QRadioButton::toggled, this, &AudioPane::SaveSettings);
  connect(m_dsp_lle, &QRadioButton::toggled, this, &AudioPane::SaveSettings);
  connect(m_dsp_interpreter, &QRadioButton::toggled, this, &AudioPane::SaveSettings);

#ifdef _WIN32
  connect(m_wasapi_device_combo,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
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
  const auto current = SConfig::GetInstance().sBackend;
  for (const auto& backend : AudioCommon::GetSoundBackends())
  {
    m_backend_combo->addItem(tr(backend.c_str()), QVariant(QString::fromStdString(backend)));
    if (backend == current)
      m_backend_combo->setCurrentIndex(m_backend_combo->count() - 1);
  }

  OnBackendChanged();

  // Volume
  OnVolumeChanged(settings.GetVolume());

  // DPL2
  m_dolby_pro_logic->setChecked(SConfig::GetInstance().bDPL2Decoder);

  // Latency
  if (m_latency_control_supported)
    m_latency_spin->setValue(SConfig::GetInstance().iLatency);

  // Stretch
  m_stretching_enable->setChecked(SConfig::GetInstance().m_audio_stretch);
  m_stretching_buffer_slider->setValue(SConfig::GetInstance().m_audio_stretch_max_latency);
  m_stretching_buffer_slider->setEnabled(m_stretching_enable->isChecked());
  m_stretching_buffer_indicator->setText(tr("%1 ms").arg(m_stretching_buffer_slider->value()));

#ifdef _WIN32
  if (SConfig::GetInstance().sWASAPIDevice == "default")
  {
    m_wasapi_device_combo->setCurrentIndex(0);
  }
  else
  {
    m_wasapi_device_combo->setCurrentText(
        QString::fromStdString(SConfig::GetInstance().sWASAPIDevice));
  }
#endif
}

void AudioPane::SaveSettings()
{
  auto& settings = Settings::Instance();

  // DSP
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
  }

  // DPL2
  SConfig::GetInstance().bDPL2Decoder = m_dolby_pro_logic->isChecked();

  // Latency
  if (m_latency_control_supported)
    SConfig::GetInstance().iLatency = m_latency_spin->value();

  // Stretch
  SConfig::GetInstance().m_audio_stretch = m_stretching_enable->isChecked();
  SConfig::GetInstance().m_audio_stretch_max_latency = m_stretching_buffer_slider->value();
  m_stretching_buffer_label->setEnabled(m_stretching_enable->isChecked());
  m_stretching_buffer_slider->setEnabled(m_stretching_enable->isChecked());
  m_stretching_buffer_indicator->setEnabled(m_stretching_enable->isChecked());
  m_stretching_buffer_indicator->setText(
      tr("%1 ms").arg(SConfig::GetInstance().m_audio_stretch_max_latency));

#ifdef _WIN32
  std::string device = "default";

  if (m_wasapi_device_combo->currentIndex() != 0)
    device = m_wasapi_device_combo->currentText().toStdString();

  SConfig::GetInstance().sWASAPIDevice = device;
#endif

  AudioCommon::UpdateSoundStream();
}

void AudioPane::OnBackendChanged()
{
  const auto backend = SConfig::GetInstance().sBackend;

  m_dolby_pro_logic->setEnabled(AudioCommon::SupportsDPL2Decoder(backend));
  if (m_latency_control_supported)
  {
    m_latency_label->setEnabled(AudioCommon::SupportsLatencyControl(backend));
    m_latency_spin->setEnabled(AudioCommon::SupportsLatencyControl(backend));
  }

#ifdef _WIN32
  bool is_wasapi = backend == BACKEND_WASAPI;
  m_wasapi_device_label->setHidden(!is_wasapi);
  m_wasapi_device_combo->setHidden(!is_wasapi);

  if (is_wasapi)
  {
    m_wasapi_device_combo->clear();
    m_wasapi_device_combo->addItem(tr("Default Device"));

    for (const auto device : WASAPIStream::GetAvailableDevices())
      m_wasapi_device_combo->addItem(QString::fromStdString(device));
  }
#endif

  m_volume_slider->setEnabled(AudioCommon::SupportsVolumeChanges(backend));
  m_volume_indicator->setEnabled(AudioCommon::SupportsVolumeChanges(backend));
}

void AudioPane::OnEmulationStateChanged(bool running)
{
  m_dsp_hle->setEnabled(!running);
  m_dsp_lle->setEnabled(!running);
  m_dsp_interpreter->setEnabled(!running);
  m_dolby_pro_logic->setEnabled(!running);
  m_backend_label->setEnabled(!running);
  m_backend_combo->setEnabled(!running);
  if (m_latency_control_supported)
  {
    m_latency_label->setEnabled(!running);
    m_latency_spin->setEnabled(!running);
  }

#ifdef _WIN32
  m_wasapi_device_combo->setEnabled(!running);
#endif
}

void AudioPane::OnVolumeChanged(int volume)
{
  m_volume_slider->setValue(volume);
  m_volume_indicator->setText(tr("%1 %").arg(volume));
}

void AudioPane::CheckNeedForLatencyControl()
{
  std::vector<std::string> backends = AudioCommon::GetSoundBackends();
  m_latency_control_supported =
      std::any_of(backends.cbegin(), backends.cend(), AudioCommon::SupportsLatencyControl);
}
