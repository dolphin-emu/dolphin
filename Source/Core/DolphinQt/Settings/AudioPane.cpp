// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/AudioPane.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
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
#include "Core/Core.h"
#include "Core/System.h"

#include "DolphinQt/Config/SettingsWindow.h"
#include "DolphinQt/Settings.h"

AudioPane::AudioPane()
{
  CheckNeedForLatencyControl();
  CreateWidgets();
  LoadSettings();
  ConnectWidgets();

  connect(&Settings::Instance(), &Settings::VolumeChanged, this, &AudioPane::OnVolumeChanged);
  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    OnEmulationStateChanged(state != Core::State::Uninitialized);
  });

  OnEmulationStateChanged(Core::GetState(Core::System::GetInstance()) !=
                          Core::State::Uninitialized);
}

void AudioPane::CreateWidgets()
{
  auto* dsp_box = new QGroupBox(tr("DSP Emulation Engine"));
  auto* dsp_layout = new QVBoxLayout;

  dsp_box->setLayout(dsp_layout);
  m_dsp_hle = new QRadioButton(tr("DSP HLE (recommended)"));
  m_dsp_lle = new QRadioButton(tr("DSP LLE Recompiler (slow)"));
  m_dsp_interpreter = new QRadioButton(tr("DSP LLE Interpreter (very slow)"));

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
    m_latency_spin->setToolTip(
        tr("Sets the latency in milliseconds. Higher values may reduce audio "
           "crackling. Certain backends only."));
  }

  m_dolby_pro_logic->setToolTip(
      tr("Enables Dolby Pro Logic II emulation using 5.1 surround. Certain backends only."));

  auto* dolby_quality_layout = new QHBoxLayout;

  m_dolby_quality_label = new QLabel(tr("Decoding Quality:"));

  m_dolby_quality_slider = new QSlider(Qt::Horizontal);
  m_dolby_quality_slider->setMinimum(0);
  m_dolby_quality_slider->setMaximum(3);
  m_dolby_quality_slider->setPageStep(1);
  m_dolby_quality_slider->setTickPosition(QSlider::TicksBelow);
  m_dolby_quality_slider->setToolTip(
      tr("Quality of the DPLII decoder. Audio latency increases with quality."));
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
  m_wasapi_device_combo = new QComboBox;

  backend_layout->addRow(m_wasapi_device_label, m_wasapi_device_combo);
#endif

  backend_layout->addRow(m_dolby_pro_logic);
  backend_layout->addRow(m_dolby_quality_label);
  backend_layout->addRow(dolby_quality_layout);
  backend_layout->addRow(m_dolby_quality_latency_label);

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

  dsp_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  auto* const main_vbox_layout = new QVBoxLayout;
  main_vbox_layout->addWidget(dsp_box);
  main_vbox_layout->addWidget(backend_box);
  main_vbox_layout->addWidget(stretching_box);

  m_main_layout = new QHBoxLayout;
  m_main_layout->addLayout(main_vbox_layout);
  m_main_layout->addWidget(volume_box);

  setLayout(m_main_layout);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void AudioPane::ConnectWidgets()
{
  connect(m_backend_combo, &QComboBox::currentIndexChanged, this, &AudioPane::SaveSettings);
  connect(m_volume_slider, &QSlider::valueChanged, this, &AudioPane::SaveSettings);
  if (m_latency_control_supported)
  {
    connect(m_latency_spin, &QSpinBox::valueChanged, this, &AudioPane::SaveSettings);
  }
  connect(m_stretching_buffer_slider, &QSlider::valueChanged, this, &AudioPane::SaveSettings);
  connect(m_dolby_pro_logic, &QCheckBox::toggled, this, &AudioPane::SaveSettings);
  connect(m_dolby_quality_slider, &QSlider::valueChanged, this, &AudioPane::SaveSettings);
  connect(m_stretching_enable, &QCheckBox::toggled, this, &AudioPane::SaveSettings);
  connect(m_dsp_hle, &QRadioButton::toggled, this, &AudioPane::SaveSettings);
  connect(m_dsp_lle, &QRadioButton::toggled, this, &AudioPane::SaveSettings);
  connect(m_dsp_interpreter, &QRadioButton::toggled, this, &AudioPane::SaveSettings);

#ifdef _WIN32
  connect(m_wasapi_device_combo, &QComboBox::currentIndexChanged, this, &AudioPane::SaveSettings);
#endif
}

void AudioPane::LoadSettings()
{
  auto& settings = Settings::Instance();

  // DSP
  if (Config::Get(Config::MAIN_DSP_HLE))
  {
    m_dsp_hle->setChecked(true);
  }
  else
  {
    m_dsp_lle->setChecked(Config::Get(Config::MAIN_DSP_JIT));
    m_dsp_interpreter->setChecked(!Config::Get(Config::MAIN_DSP_JIT));
  }

  // Backend
  const auto current = Config::Get(Config::MAIN_AUDIO_BACKEND);
  bool selection_set = false;
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

  OnBackendChanged();

  // Volume
  OnVolumeChanged(settings.GetVolume());

  // DPL2
  m_dolby_pro_logic->setChecked(Config::Get(Config::MAIN_DPL2_DECODER));
  m_dolby_quality_slider->setValue(int(Config::Get(Config::MAIN_DPL2_QUALITY)));
  m_dolby_quality_latency_label->setText(
      GetDPL2ApproximateLatencyLabel(Config::Get(Config::MAIN_DPL2_QUALITY)));
  if (AudioCommon::SupportsDPL2Decoder(current) && !m_dsp_hle->isChecked())
  {
    EnableDolbyQualityWidgets(m_dolby_pro_logic->isChecked());
  }

  // Latency
  if (m_latency_control_supported)
    m_latency_spin->setValue(Config::Get(Config::MAIN_AUDIO_LATENCY));

  // Stretch
  m_stretching_enable->setChecked(Config::Get(Config::MAIN_AUDIO_STRETCH));
  m_stretching_buffer_label->setEnabled(m_stretching_enable->isChecked());
  m_stretching_buffer_slider->setValue(Config::Get(Config::MAIN_AUDIO_STRETCH_LATENCY));
  m_stretching_buffer_slider->setEnabled(m_stretching_enable->isChecked());
  m_stretching_buffer_indicator->setEnabled(m_stretching_enable->isChecked());
  m_stretching_buffer_indicator->setText(tr("%1 ms").arg(m_stretching_buffer_slider->value()));

#ifdef _WIN32
  if (Config::Get(Config::MAIN_WASAPI_DEVICE) == "default")
  {
    m_wasapi_device_combo->setCurrentIndex(0);
  }
  else
  {
    m_wasapi_device_combo->setCurrentText(
        QString::fromStdString(Config::Get(Config::MAIN_WASAPI_DEVICE)));
  }
#endif
}

void AudioPane::SaveSettings()
{
  auto& settings = Settings::Instance();

  // DSP
  if (Config::Get(Config::MAIN_DSP_HLE) != m_dsp_hle->isChecked() ||
      Config::Get(Config::MAIN_DSP_JIT) != m_dsp_lle->isChecked())
  {
    OnDspChanged();
  }
  Config::SetBaseOrCurrent(Config::MAIN_DSP_HLE, m_dsp_hle->isChecked());
  Config::SetBaseOrCurrent(Config::MAIN_DSP_JIT, m_dsp_lle->isChecked());

  // Backend
  const auto selection =
      m_backend_combo->itemData(m_backend_combo->currentIndex()).toString().toStdString();
  std::string backend = Config::Get(Config::MAIN_AUDIO_BACKEND);

  if (selection != backend)
  {
    backend = selection;
    Config::SetBaseOrCurrent(Config::MAIN_AUDIO_BACKEND, selection);
    OnBackendChanged();
  }

  // Volume
  if (m_volume_slider->value() != settings.GetVolume())
  {
    settings.SetVolume(m_volume_slider->value());
    OnVolumeChanged(settings.GetVolume());
  }

  // DPL2
  Config::SetBaseOrCurrent(Config::MAIN_DPL2_DECODER, m_dolby_pro_logic->isChecked());
  Config::SetBase(Config::MAIN_DPL2_QUALITY,
                  static_cast<AudioCommon::DPL2Quality>(m_dolby_quality_slider->value()));
  m_dolby_quality_latency_label->setText(
      GetDPL2ApproximateLatencyLabel(Config::Get(Config::MAIN_DPL2_QUALITY)));
  if (AudioCommon::SupportsDPL2Decoder(backend) && !m_dsp_hle->isChecked())
  {
    EnableDolbyQualityWidgets(m_dolby_pro_logic->isChecked());
  }

  // Latency
  if (m_latency_control_supported)
    Config::SetBaseOrCurrent(Config::MAIN_AUDIO_LATENCY, m_latency_spin->value());

  // Stretch
  Config::SetBaseOrCurrent(Config::MAIN_AUDIO_STRETCH, m_stretching_enable->isChecked());
  Config::SetBaseOrCurrent(Config::MAIN_AUDIO_STRETCH_LATENCY, m_stretching_buffer_slider->value());
  m_stretching_buffer_label->setEnabled(m_stretching_enable->isChecked());
  m_stretching_buffer_slider->setEnabled(m_stretching_enable->isChecked());
  m_stretching_buffer_indicator->setEnabled(m_stretching_enable->isChecked());
  m_stretching_buffer_indicator->setText(
      tr("%1 ms").arg(Config::Get(Config::MAIN_AUDIO_STRETCH_LATENCY)));

#ifdef _WIN32
  std::string device = "default";

  if (m_wasapi_device_combo->currentIndex() != 0)
    device = m_wasapi_device_combo->currentText().toStdString();

  Config::SetBaseOrCurrent(Config::MAIN_WASAPI_DEVICE, device);
#endif

  AudioCommon::UpdateSoundStream(Core::System::GetInstance());
}

void AudioPane::OnDspChanged()
{
  const auto backend = Config::Get(Config::MAIN_AUDIO_BACKEND);

  m_dolby_pro_logic->setEnabled(AudioCommon::SupportsDPL2Decoder(backend) &&
                                !m_dsp_hle->isChecked());
  EnableDolbyQualityWidgets(AudioCommon::SupportsDPL2Decoder(backend) && !m_dsp_hle->isChecked() &&
                            m_dolby_pro_logic->isChecked());
}

void AudioPane::OnBackendChanged()
{
  const auto backend = Config::Get(Config::MAIN_AUDIO_BACKEND);

  m_dolby_pro_logic->setEnabled(AudioCommon::SupportsDPL2Decoder(backend) &&
                                !m_dsp_hle->isChecked());
  EnableDolbyQualityWidgets(AudioCommon::SupportsDPL2Decoder(backend) && !m_dsp_hle->isChecked() &&
                            m_dolby_pro_logic->isChecked());
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
  m_backend_label->setEnabled(!running);
  m_backend_combo->setEnabled(!running);
  if (AudioCommon::SupportsDPL2Decoder(Config::Get(Config::MAIN_AUDIO_BACKEND)) &&
      !m_dsp_hle->isChecked())
  {
    m_dolby_pro_logic->setEnabled(!running);
    EnableDolbyQualityWidgets(!running && m_dolby_pro_logic->isChecked());
  }
  if (m_latency_control_supported &&
      AudioCommon::SupportsLatencyControl(Config::Get(Config::MAIN_AUDIO_BACKEND)))
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
  m_volume_indicator->setText(tr("%1%").arg(volume));
}

void AudioPane::CheckNeedForLatencyControl()
{
  std::vector<std::string> backends = AudioCommon::GetSoundBackends();
  m_latency_control_supported = std::ranges::any_of(backends, AudioCommon::SupportsLatencyControl);
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
  default:
    return tr("High");
  }
}

QString AudioPane::GetDPL2ApproximateLatencyLabel(AudioCommon::DPL2Quality value) const
{
  switch (value)
  {
  case AudioCommon::DPL2Quality::Lowest:
    return tr("Latency: ~10 ms");
  case AudioCommon::DPL2Quality::Low:
    return tr("Latency: ~20 ms");
  case AudioCommon::DPL2Quality::Highest:
    return tr("Latency: ~80 ms");
  default:
    return tr("Latency: ~40 ms");
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
