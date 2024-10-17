// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/AudioPane.h"

#include <QFontMetrics>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpacerItem>
#include <QVBoxLayout>

#include "AudioCommon/AudioCommon.h"
#include "AudioCommon/Enums.h"
#include "AudioCommon/WASAPIStream.h"

#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/System.h"
#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/Config/ConfigControls/ConfigChoice.h"
#include "DolphinQt/Config/ConfigControls/ConfigRadio.h"
#include "DolphinQt/Config/ConfigControls/ConfigSlider.h"

#include "DolphinQt/Config/SettingsWindow.h"
#include "DolphinQt/Settings.h"

AudioPane::AudioPane()
{
  CheckNeedForLatencyControl();
  CreateWidgets();
  AddDescriptions();
  ConnectWidgets();
  OnBackendChanged();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    OnEmulationStateChanged(state != Core::State::Uninitialized);
  });

  OnEmulationStateChanged(!Core::IsUninitialized(Core::System::GetInstance()));
}

void AudioPane::CreateWidgets()
{
  auto* dsp_box = new QGroupBox(tr("DSP Emulation Engine"));
  auto* dsp_layout = new QVBoxLayout;

  dsp_box->setLayout(dsp_layout);

  m_dsp_hle = new ConfigRadioBool(tr("DSP HLE (recommended)"), Config::MAIN_DSP_HLE, true);
  m_dsp_lle = new ConfigRadioBool(tr("DSP LLE Recompiler (slow)"), Config::MAIN_DSP_JIT, true);
  // Selecting this sets all other options to false.
  m_dsp_interpreter =
      new ConfigRadioBool(tr("DSP LLE Interpreter (very slow)"), Config::MAIN_DSP_JIT);

  dsp_layout->addStretch(1);
  dsp_layout->addWidget(m_dsp_hle);
  dsp_layout->addWidget(m_dsp_lle);
  dsp_layout->addWidget(m_dsp_interpreter);
  dsp_layout->addStretch(1);

  auto* volume_box = new QGroupBox(tr("Volume"));
  auto* volume_layout = new QVBoxLayout;
  m_volume_slider = new ConfigSlider(0, 100, Config::MAIN_AUDIO_VOLUME);
  m_volume_indicator = new QLabel(tr("%1 %").arg(m_volume_slider->value()));

  volume_box->setLayout(volume_layout);

  m_volume_slider->setOrientation(Qt::Vertical);

  m_volume_indicator->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
  m_volume_indicator->setFixedWidth(QFontMetrics(font()).boundingRect(tr("%1 %").arg(100)).width());

  volume_layout->addWidget(m_volume_slider, 0, Qt::AlignHCenter);
  volume_layout->addWidget(m_volume_indicator, 0, Qt::AlignHCenter);

  auto* backend_box = new QGroupBox(tr("Backend Settings"));
  auto* backend_layout = new QFormLayout;
  backend_box->setLayout(backend_layout);
  m_backend_label = new QLabel(tr("Audio Backend:"));
  m_backend_combo =
      new ConfigStringChoice(AudioCommon::GetSoundBackends(), Config::MAIN_AUDIO_BACKEND);
  m_dolby_pro_logic = new ConfigBool(tr("Dolby Pro Logic II Decoder"), Config::MAIN_DPL2_DECODER);
  m_dolby_quality_label = new QLabel(tr("Decoding Quality:"));

  QStringList quality_options{tr("Lowest (Latency ~10 ms)"), tr("Low (Latency ~20 ms)"),
                              tr("High (Latency ~40 ms)"), tr("Highest (Latency ~80 ms)")};

  m_dolby_quality_combo = new ConfigChoice(quality_options, Config::MAIN_DPL2_QUALITY);

  backend_layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  backend_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  backend_layout->addRow(m_backend_label, m_backend_combo);

#ifdef _WIN32
  std::vector<std::pair<QString, QString>> wasapi_options;
  wasapi_options.push_back(
      std::pair<QString, QString>{tr("Default Device"), QStringLiteral("default")});

  for (auto string : WASAPIStream::GetAvailableDevices())
  {
    wasapi_options.push_back(std::pair<QString, QString>{QString::fromStdString(string),
                                                         QString::fromStdString(string)});
  }

  m_wasapi_device_label = new QLabel(tr("Output Device:"));
  m_wasapi_device_combo = new ConfigStringChoice(wasapi_options, Config::MAIN_WASAPI_DEVICE);

  backend_layout->addRow(m_wasapi_device_label, m_wasapi_device_combo);
#endif

  if (m_latency_control_supported)
  {
    m_latency_slider = new ConfigSlider(0, 200, Config::MAIN_AUDIO_LATENCY);
    m_latency_label = new QLabel(tr("Latency: %1 ms").arg(m_latency_slider->value()));
    m_latency_label->setFixedWidth(
        QFontMetrics(font()).boundingRect(tr("Latency:  000 ms")).width());

    backend_layout->addRow(m_latency_label, m_latency_slider);
  }

  backend_layout->addRow(m_dolby_pro_logic);
  backend_layout->addRow(m_dolby_quality_label, m_dolby_quality_combo);

  auto* stretching_box = new QGroupBox(tr("Audio Stretching Settings"));
  auto* stretching_layout = new QGridLayout;
  m_stretching_enable = new ConfigBool(tr("Enable Audio Stretching"), Config::MAIN_AUDIO_STRETCH);
  m_stretching_buffer_slider = new ConfigSlider(5, 300, Config::MAIN_AUDIO_STRETCH_LATENCY);
  m_stretching_buffer_indicator = new QLabel(tr("%1 ms").arg(m_stretching_buffer_slider->value()));
  m_stretching_buffer_label = new QLabel(tr("Buffer Size:"));
  stretching_box->setLayout(stretching_layout);

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
  // AudioCommon::UpdateSoundStream needs to be called on anything that can change while a game is
  // running.  This only applies to volume and stretching right now.
  connect(m_backend_combo, &QComboBox::currentIndexChanged, this, &AudioPane::OnBackendChanged);
  connect(m_dolby_pro_logic, &ConfigBool::toggled, this, &AudioPane::OnDspChanged);
  connect(m_dsp_hle, &ConfigRadioBool::OnSelected, this, &AudioPane::OnDspChanged);
  connect(m_dsp_lle, &ConfigRadioBool::OnSelected, this, &AudioPane::OnDspChanged);
  connect(m_dsp_interpreter, &QRadioButton::toggled, this, [this]() {
    if (m_dsp_interpreter->isChecked())
      OnDspChanged();
  });

  connect(m_volume_slider, &QSlider::valueChanged, this, [this](int value) {
    m_volume_indicator->setText(tr("%1%").arg(value));
    AudioCommon::UpdateSoundStream(Core::System::GetInstance());
  });
  connect(m_latency_slider, &QSlider::valueChanged, this,
          [this](int value) { m_latency_label->setText(tr("Latency: %1 ms").arg(value)); });

  connect(m_stretching_enable, &QCheckBox::toggled, this, [this](bool checked) {
    m_stretching_buffer_label->setEnabled(checked);
    m_stretching_buffer_slider->setEnabled(checked);
    m_stretching_buffer_indicator->setEnabled(checked);
    AudioCommon::UpdateSoundStream(Core::System::GetInstance());
  });
  connect(m_stretching_buffer_slider, &QSlider::valueChanged, this, [this](int value) {
    m_stretching_buffer_indicator->setText(tr("%1 ms").arg(value));
    AudioCommon::UpdateSoundStream(Core::System::GetInstance());
  });
}

void AudioPane::OnDspChanged()
{
  const auto backend = Config::Get(Config::MAIN_AUDIO_BACKEND);
  const bool enabled = AudioCommon::SupportsDPL2Decoder(backend) && !m_dsp_hle->isChecked();
  m_dolby_pro_logic->setEnabled(enabled);
  m_dolby_quality_label->setEnabled(enabled && m_dolby_pro_logic->isChecked());
  m_dolby_quality_combo->setEnabled(enabled && m_dolby_pro_logic->isChecked());
}

void AudioPane::OnBackendChanged()
{
  if (!m_dsp_hle->isChecked() && !m_dsp_lle->isChecked())
    m_dsp_interpreter->setChecked(true);

  OnDspChanged();

  const auto backend = Config::Get(Config::MAIN_AUDIO_BACKEND);

  if (m_latency_control_supported)
  {
    m_latency_label->setEnabled(AudioCommon::SupportsLatencyControl(backend));
    m_latency_slider->setEnabled(AudioCommon::SupportsLatencyControl(backend));
  }

#ifdef _WIN32
  bool is_wasapi = backend == BACKEND_WASAPI;
  m_wasapi_device_label->setHidden(!is_wasapi);
  m_wasapi_device_combo->setHidden(!is_wasapi);
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
    m_dolby_quality_label->setEnabled(!running && m_dolby_pro_logic->isChecked());
    m_dolby_quality_combo->setEnabled(!running && m_dolby_pro_logic->isChecked());
  }
  if (m_latency_control_supported &&
      AudioCommon::SupportsLatencyControl(Config::Get(Config::MAIN_AUDIO_BACKEND)))
  {
    m_latency_label->setEnabled(!running);
    m_latency_slider->setEnabled(!running);
  }

#ifdef _WIN32
  m_wasapi_device_combo->setEnabled(!running);
#endif
}

void AudioPane::CheckNeedForLatencyControl()
{
  std::vector<std::string> backends = AudioCommon::GetSoundBackends();
  m_latency_control_supported =
      std::any_of(backends.cbegin(), backends.cend(), AudioCommon::SupportsLatencyControl);
}

void AudioPane::AddDescriptions()
{
  static const char TR_HLE_DESCRIPTION[] = QT_TR_NOOP(
      "High Level Emulation of the DSP. Fast, but not always accurate. Lacks Dolby Pro Logic "
      "II decoding.<br><br><dolphin_emphasis>If unsure, select this mode.</dolphin_emphasis>");
  static const char TR_LLE_DESCRIPTION[] =
      QT_TR_NOOP("Low Level Emulation of the DSP, via a recompiler. Slower, but more accurate. "
                 "Enables Dolby Pro Logic II decoding on certain audio backends.");
  static const char TR_INTERPRETER_DESCRIPTION[] =
      QT_TR_NOOP("Low Level Emulation of the DSP, via an interpreter. Slowest, for debugging "
                 "purposes only.<br><br><dolphin_emphasis>Not recommended; consider using DSP HLE "
                 "or DSP LLE Recompiler instead.</dolphin_emphasis>");
  static const char TR_AUDIO_BACKEND_DESCRIPTION[] =
      QT_TR_NOOP("Selects which audio API to use internally.<br><br><dolphin_emphasis>If unsure, "
                 "select %1.</dolphin_emphasis>");
  static const char TR_WASAPI_DEVICE_DESCRIPTION[] =
      QT_TR_NOOP("Selects an output device to use.<br><br><dolphin_emphasis>If unsure, select "
                 "Default Device.</dolphin_emphasis>");
  static const char TR_LATENCY_SLIDER_DESCRIPTION[] = QT_TR_NOOP(
      "Sets the audio latency in milliseconds. Higher values may reduce audio crackling. Certain "
      "backends only.<br><br><dolphin_emphasis>If unsure, leave this at 20 ms.</dolphin_emphasis>");
  static const char TR_DOLBY_DESCRIPTION[] =
      QT_TR_NOOP("Enables Dolby Pro Logic II emulation using 5.1 surround. Certain backends only. "
                 "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_DOLBY_OPTIONS_DESCRIPTION[] = QT_TR_NOOP(
      "Adjusts the quality setting of the Dolby Pro Logic II decoder. Higher presets increases "
      "audio latency.<br><br><dolphin_emphasis>If unsure, select High.</dolphin_emphasis>");
  static const char TR_STRETCH_ENABLE_DESCRIPTION[] = QT_TR_NOOP(
      "Enables stretching of the audio to match emulation speed. <br><br><dolphin_emphasis>If "
      "unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_STRETCH_SLIDER_DESCRIPTION[] = QT_TR_NOOP(
      "Size of the stretch buffer in milliseconds. Lower values may cause audio "
      "crackling.<br><br><dolphin_emphasis>If unsure, leave this at 80 ms.</dolphin_emphasis>");
  static const char TR_VOLUME_DESCRIPTION[] =
      QT_TR_NOOP("Adjusts audio output volume.<br><br><dolphin_emphasis>If unsure, leave this at "
                 "100%.</dolphin_emphasis>");

  m_dsp_hle->SetTitle(tr("DSP HLE"));
  m_dsp_hle->SetDescription(tr(TR_HLE_DESCRIPTION));
  m_dsp_lle->SetTitle(tr("DSP LLE Recompiler"));
  m_dsp_lle->SetDescription(tr(TR_LLE_DESCRIPTION));
  m_dsp_interpreter->SetTitle(tr("DSP LLE Interpreter"));
  m_dsp_interpreter->SetDescription(tr(TR_INTERPRETER_DESCRIPTION));

  m_backend_combo->SetTitle(tr("Audio Backend"));
  m_backend_combo->SetDescription(
      tr(TR_AUDIO_BACKEND_DESCRIPTION)
          .arg(QString::fromStdString(AudioCommon::GetDefaultSoundBackend())));

  m_dolby_pro_logic->SetTitle(tr("Dolby Pro Logic II Decoder"));
  m_dolby_pro_logic->SetDescription(tr(TR_DOLBY_DESCRIPTION));

  m_dolby_quality_combo->SetTitle(tr("Decoding Quality"));
  m_dolby_quality_combo->SetDescription(tr(TR_DOLBY_OPTIONS_DESCRIPTION));

#ifdef _WIN32
  m_wasapi_device_combo->SetTitle(tr("Output Device"));
  m_wasapi_device_combo->SetDescription(tr(TR_WASAPI_DEVICE_DESCRIPTION));
#endif

  m_stretching_enable->SetTitle(tr("Enable Audio Stretching"));
  m_stretching_enable->SetDescription(tr(TR_STRETCH_ENABLE_DESCRIPTION));

  m_stretching_buffer_slider->SetTitle(tr("Buffer Size"));
  m_stretching_buffer_slider->SetDescription(tr(TR_STRETCH_SLIDER_DESCRIPTION));

  m_volume_slider->SetTitle(tr("Volume"));
  m_volume_slider->SetDescription(tr(TR_VOLUME_DESCRIPTION));

  if (m_latency_control_supported)
  {
    m_latency_slider->SetTitle(tr("Latency"));
    m_latency_slider->SetDescription(tr(TR_LATENCY_SLIDER_DESCRIPTION));
  }
}
