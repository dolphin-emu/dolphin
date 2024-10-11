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
  auto* dsp_box = new QGroupBox(tr("DSP Options"));
  auto* dsp_layout = new QHBoxLayout;

  dsp_box->setLayout(dsp_layout);
  QLabel* dsp_combo_label = new QLabel(tr("DSP Emulation Engine:"));
  m_dsp_combo = new ConfigComplexChoice(Config::MAIN_DSP_HLE, Config::MAIN_DSP_JIT);
  m_dsp_combo->Add(tr("HLE (recommended)"), true, true);
  m_dsp_combo->Add(tr("LLE Recompiler (slow)"), false, true);
  m_dsp_combo->Add(tr("LLE Interpreter (very slow)"), false, false);
  m_dsp_combo->Refresh();
  // The state true/false shouldn't normally happen and forces no option to be selected.
  if (m_dsp_combo->currentIndex() == -1)
    m_dsp_combo->setCurrentIndex(0);

  dsp_layout->addWidget(dsp_combo_label);
  dsp_layout->addWidget(m_dsp_combo, Qt::AlignLeft);

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

  dsp_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  auto* misc_box = new QGroupBox(tr("Miscellaneous Settings"));
  auto* misc_layout = new QGridLayout;
  misc_box->setLayout(misc_layout);

  m_audio_fill_gaps = new ConfigBool(tr("Fill Audio Gaps"), Config::MAIN_AUDIO_FILL_GAPS);

  m_speed_up_mute_enable = new ConfigBool(tr("Mute When Disabling Speed Limit"),
                                          Config::MAIN_AUDIO_MUTE_ON_DISABLED_SPEED_LIMIT);

  misc_layout->addWidget(m_audio_fill_gaps, 0, 0, 1, 1);
  misc_layout->addWidget(m_speed_up_mute_enable, 1, 0, 1, 1);

  auto* const main_vbox_layout = new QVBoxLayout;

  main_vbox_layout->addWidget(dsp_box);
  main_vbox_layout->addWidget(backend_box);
  main_vbox_layout->addWidget(misc_box);

  m_main_layout = new QHBoxLayout;
  m_main_layout->addLayout(main_vbox_layout);
  m_main_layout->addWidget(volume_box);

  setLayout(m_main_layout);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void AudioPane::ConnectWidgets()
{
  connect(m_backend_combo, &QComboBox::currentIndexChanged, this, &AudioPane::OnBackendChanged);
  connect(m_dolby_pro_logic, &ConfigBool::toggled, this, &AudioPane::OnDspChanged);
  connect(m_dsp_combo, &ConfigComplexChoice::currentIndexChanged, this, &AudioPane::OnDspChanged);
  connect(m_volume_slider, &QSlider::valueChanged, this, [this](int value) {
    m_volume_indicator->setText(tr("%1%").arg(value));
    AudioCommon::UpdateSoundStream(Core::System::GetInstance());
  });
  connect(m_latency_slider, &QSlider::valueChanged, this,
          [this](int value) { m_latency_label->setText(tr("Latency: %1 ms").arg(value)); });
}

void AudioPane::OnDspChanged()
{
  const auto backend = Config::Get(Config::MAIN_AUDIO_BACKEND);
  const bool enabled =
      AudioCommon::SupportsDPL2Decoder(backend) && !Config::Get(Config::MAIN_DSP_HLE);
  m_dolby_pro_logic->setEnabled(enabled);
  m_dolby_quality_label->setEnabled(enabled && m_dolby_pro_logic->isChecked());
  m_dolby_quality_combo->setEnabled(enabled && m_dolby_pro_logic->isChecked());
}

void AudioPane::OnBackendChanged()
{
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
  m_dsp_combo->setEnabled(!running);
  m_backend_label->setEnabled(!running);
  m_backend_combo->setEnabled(!running);
  if (AudioCommon::SupportsDPL2Decoder(Config::Get(Config::MAIN_AUDIO_BACKEND)) &&
      !Config::Get(Config::MAIN_DSP_HLE))
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
  m_latency_control_supported = std::ranges::any_of(backends, AudioCommon::SupportsLatencyControl);
}

void AudioPane::AddDescriptions()
{
  static const char TR_DSP_DESCRIPTION[] = QT_TR_NOOP(
      "Selects how the Digital Signal Processor (DSP) is emulated. Determines how the audio is "
      "processed and what system features are available.<br><br>"
      "<b>HLE</b> - High Level Emulation of the DSP. Fast, but not always accurate. Lacks Dolby "
      "Pro Logic II decoding.<br><br>"
      "<b>LLE Recompiler</b> - Low Level Emulation of the DSP, via a recompiler. Slower, but more "
      "accurate. Enables Dolby Pro Logic II decoding on certain audio backends.<br><br>"
      "<b>LLE Interpreter</b> - Low Level Emulation of the DSP, via an interpreter. Slowest, for "
      "debugging purposes only. Not recommended.<br><br><dolphin_emphasis>If unsure, select "
      "HLE.</dolphin_emphasis>");
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
  static const char TR_VOLUME_DESCRIPTION[] =
      QT_TR_NOOP("Adjusts audio output volume.<br><br><dolphin_emphasis>If unsure, leave this at "
                 "100%.</dolphin_emphasis>");
  static const char TR_FILL_AUDIO_GAPS_DESCRIPTION[] = QT_TR_NOOP(
      "Repeat existing audio during lag spikes to prevent stuttering.<br><br><dolphin_emphasis>If "
      "unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_SPEED_UP_MUTE_DESCRIPTION[] =
      QT_TR_NOOP("Mutes the audio when overriding the emulation speed limit (default hotkey: Tab). "
                 "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");

  m_dsp_combo->SetTitle(tr("DSP Emulation Engine"));
  m_dsp_combo->SetDescription(tr(TR_DSP_DESCRIPTION));

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
  m_volume_slider->SetTitle(tr("Volume"));
  m_volume_slider->SetDescription(tr(TR_VOLUME_DESCRIPTION));

  if (m_latency_control_supported)
  {
    m_latency_slider->SetTitle(tr("Latency"));
    m_latency_slider->SetDescription(tr(TR_LATENCY_SLIDER_DESCRIPTION));
  }

  m_speed_up_mute_enable->SetTitle(tr("Mute When Disabling Speed Limit"));
  m_speed_up_mute_enable->SetDescription(tr(TR_SPEED_UP_MUTE_DESCRIPTION));

  m_audio_fill_gaps->SetTitle(tr("Fill Audio Gaps"));
  m_audio_fill_gaps->SetDescription(tr(TR_FILL_AUDIO_GAPS_DESCRIPTION));
}
