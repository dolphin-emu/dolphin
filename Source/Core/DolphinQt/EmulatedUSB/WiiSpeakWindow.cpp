// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/EmulatedUSB/WiiSpeakWindow.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>

#ifdef HAVE_CUBEB
#include "AudioCommon/CubebUtils.h"
#endif
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/System.h"
#include "DolphinQt/Settings.h"

WiiSpeakWindow::WiiSpeakWindow(QWidget* parent) : QWidget(parent)
{
  // i18n: Window for managing the Wii Speak microphone
  setWindowTitle(tr("Wii Speak Manager"));
  setObjectName(QStringLiteral("wii_speak_manager"));
  setMinimumSize(QSize(700, 200));

  CreateMainWindow();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &WiiSpeakWindow::OnEmulationStateChanged);

  OnEmulationStateChanged(Core::GetState(Core::System::GetInstance()));
}

WiiSpeakWindow::~WiiSpeakWindow() = default;

void WiiSpeakWindow::CreateMainWindow()
{
  auto* main_layout = new QVBoxLayout();
  auto* label = new QLabel();
  label->setText(QStringLiteral("<center><i>%1</i></center>")
                     .arg(tr("Some settings cannot be changed when emulation is running.")));
  main_layout->addWidget(label);

  auto* checkbox_group = new QGroupBox();
  auto* checkbox_layout = new QHBoxLayout();
  checkbox_layout->setAlignment(Qt::AlignHCenter);
  m_checkbox_enabled = new QCheckBox(tr("Emulate Wii Speak"), this);
  m_checkbox_enabled->setChecked(Config::Get(Config::MAIN_EMULATE_WII_SPEAK));
  connect(m_checkbox_enabled, &QCheckBox::toggled, this, &WiiSpeakWindow::EmulateWiiSpeak);
  checkbox_layout->addWidget(m_checkbox_enabled);
  checkbox_group->setLayout(checkbox_layout);
  main_layout->addWidget(checkbox_group);

  auto* config_group = new QGroupBox(tr("Microphone Configuration"));
  auto* config_layout = new QHBoxLayout();

  auto checkbox_mic_muted = new QCheckBox(tr("Mute"), this);
  checkbox_mic_muted->setChecked(Config::Get(Config::MAIN_WII_SPEAK_MUTED));
  connect(checkbox_mic_muted, &QCheckBox::toggled, this, &WiiSpeakWindow::SetWiiSpeakMuted);
  checkbox_mic_muted->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
  config_layout->addWidget(checkbox_mic_muted);

  auto* volume_layout = new QGridLayout();
  static constexpr int FILTER_MIN = -50;
  static constexpr int FILTER_MAX = 50;
  const int volume_modifier =
      std::clamp<int>(Config::Get(Config::MAIN_WII_SPEAK_VOLUME_MODIFIER), FILTER_MIN, FILTER_MAX);
  auto filter_slider = new QSlider(Qt::Horizontal, this);
  auto slider_label = new QLabel(tr("Volume modifier (value: %1dB)").arg(volume_modifier));
  connect(filter_slider, &QSlider::valueChanged, this, [slider_label](int value) {
    Config::SetBaseOrCurrent(Config::MAIN_WII_SPEAK_VOLUME_MODIFIER, value);
    slider_label->setText(tr("Volume modifier (value: %1dB)").arg(value));
  });
  filter_slider->setMinimum(FILTER_MIN);
  filter_slider->setMaximum(FILTER_MAX);
  filter_slider->setValue(volume_modifier);
  filter_slider->setTickPosition(QSlider::TicksBothSides);
  filter_slider->setTickInterval(10);
  filter_slider->setSingleStep(1);
  volume_layout->addWidget(new QLabel(QStringLiteral("%1dB").arg(FILTER_MIN)), 0, 0, Qt::AlignLeft);
  volume_layout->addWidget(slider_label, 0, 1, Qt::AlignCenter);
  volume_layout->addWidget(new QLabel(QStringLiteral("%1dB").arg(FILTER_MAX)), 0, 2,
                           Qt::AlignRight);
  volume_layout->addWidget(filter_slider, 1, 0, 1, 3);
  config_layout->addLayout(volume_layout);
  config_layout->setStretch(1, 3);

  m_combobox_microphones = new QComboBox();
#ifndef HAVE_CUBEB
  m_combobox_microphones->addItem(QLatin1String("(%1)").arg(tr("Audio backend unsupported")),
                                  QString{});
#else
  m_combobox_microphones->addItem(QLatin1String("(%1)").arg(tr("Autodetect preferred microphone")),
                                  QString{});
  for (auto& [device_id, device_name] : CubebUtils::ListInputDevices())
  {
    const auto user_data = QString::fromStdString(device_id);
    m_combobox_microphones->addItem(QString::fromStdString(device_name), user_data);
  }
#endif
  connect(m_combobox_microphones, &QComboBox::currentIndexChanged, this,
          &WiiSpeakWindow::OnInputDeviceChange);

  auto current_device_id = QString::fromStdString(Config::Get(Config::MAIN_WII_SPEAK_MICROPHONE));
  m_combobox_microphones->setCurrentIndex(m_combobox_microphones->findData(current_device_id));
  config_layout->addWidget(m_combobox_microphones);

  config_group->setLayout(config_layout);
  main_layout->addWidget(config_group);

  setLayout(main_layout);
}

void WiiSpeakWindow::EmulateWiiSpeak(bool emulate)
{
  Config::SetBaseOrCurrent(Config::MAIN_EMULATE_WII_SPEAK, emulate);
}

void WiiSpeakWindow::SetWiiSpeakMuted(bool muted)
{
  Config::SetBaseOrCurrent(Config::MAIN_WII_SPEAK_MUTED, muted);
}

void WiiSpeakWindow::OnEmulationStateChanged(Core::State state)
{
  const bool running = state != Core::State::Uninitialized;

  m_checkbox_enabled->setEnabled(!running);
  m_combobox_microphones->setEnabled(!running);
}

void WiiSpeakWindow::OnInputDeviceChange()
{
  auto user_data = m_combobox_microphones->currentData();
  if (!user_data.isValid())
    return;

  const std::string device_id = user_data.toString().toStdString();
  Config::SetBaseOrCurrent(Config::MAIN_WII_SPEAK_MICROPHONE, device_id);
}
