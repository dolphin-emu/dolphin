// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/EmulatedUSB/LogitechMicWindow.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>

#ifdef HAVE_CUBEB
#include "AudioCommon/CubebUtils.h"
#endif
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/System.h"
#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

LogitechMicWindow::LogitechMicWindow(QWidget* parent) : QWidget(parent)
{
  setWindowTitle(tr("Logitech USB Microphone Manager"));
  setWindowIcon(Resources::GetAppIcon());
  setObjectName(QStringLiteral("logitech_mic_manager"));
  setMinimumSize(QSize(700, 200));

  CreateMainWindow();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &LogitechMicWindow::OnEmulationStateChanged);

  OnEmulationStateChanged(Core::GetState(Core::System::GetInstance()));
}

void LogitechMicWindow::CreateMainWindow()
{
  auto* const main_layout = new QVBoxLayout;
  auto* const label = new QLabel;
  label->setText(QStringLiteral("<center><i>%1</i></center>")
                     .arg(tr("Some settings cannot be changed when emulation is running.")));
  main_layout->addWidget(label);

  CreateCheckboxGroup(main_layout);

  CreateMicrophoneConfigurationGroup(main_layout);

  setLayout(main_layout);
}

void LogitechMicWindow::CreateCheckboxGroup(QVBoxLayout* main_layout)
{
  auto* checkbox_group = new QGroupBox();
  auto* checkbox_layout = new QHBoxLayout();
  checkbox_layout->setAlignment(Qt::AlignHCenter);

  for (std::size_t index = 0; index != Config::EMULATED_LOGITECH_MIC_COUNT; ++index)
  {
    m_mic_enabled_checkboxes[index] = new ConfigBool(
        tr("Emulate Logitech USB Mic %1").arg(index + 1), Config::MAIN_EMULATE_LOGITECH_MIC[index]);
    checkbox_layout->addWidget(m_mic_enabled_checkboxes[index]);
  }

  checkbox_group->setLayout(checkbox_layout);
  main_layout->addWidget(checkbox_group);
}

void LogitechMicWindow::CreateMicrophoneConfigurationGroup(QVBoxLayout* main_layout)
{
  auto* main_config_group = new QGroupBox(tr("Microphone Configuration"));
  auto* main_config_layout = new QVBoxLayout();

  for (std::size_t index = 0; index != Config::EMULATED_LOGITECH_MIC_COUNT; ++index)
  {
    // i18n: %1 is a number from 1 to 4.
    auto* config_group = new QGroupBox(tr("Microphone %1 Configuration").arg(index + 1));
    auto* const config_layout = new QHBoxLayout();

    auto* const mic_muted = new ConfigBool(tr("Mute"), Config::MAIN_LOGITECH_MIC_MUTED[index]);
    mic_muted->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    config_layout->addWidget(mic_muted);

    static constexpr int FILTER_MIN = -50;
    static constexpr int FILTER_MAX = 50;

    auto* volume_layout = new QGridLayout();
    const int volume_modifier = std::clamp<int>(
        Config::Get(Config::MAIN_LOGITECH_MIC_VOLUME_MODIFIER[index]), FILTER_MIN, FILTER_MAX);
    auto* const filter_slider = new QSlider(Qt::Horizontal, this);
    auto* const slider_label = new QLabel(tr("Volume modifier (value: %1dB)").arg(volume_modifier));
    connect(filter_slider, &QSlider::valueChanged, this, [slider_label, index](int value) {
      Config::SetBaseOrCurrent(Config::MAIN_LOGITECH_MIC_VOLUME_MODIFIER[index], value);
      slider_label->setText(tr("Volume modifier (value: %1dB)").arg(value));
    });
    filter_slider->setMinimum(FILTER_MIN);
    filter_slider->setMaximum(FILTER_MAX);
    filter_slider->setValue(volume_modifier);
    filter_slider->setTickPosition(QSlider::TicksBothSides);
    filter_slider->setTickInterval(10);
    filter_slider->setSingleStep(1);
    volume_layout->addWidget(new QLabel(QStringLiteral("%1dB").arg(FILTER_MIN)), 0, 0,
                             Qt::AlignLeft);
    volume_layout->addWidget(slider_label, 0, 1, Qt::AlignCenter);
    volume_layout->addWidget(new QLabel(QStringLiteral("%1dB").arg(FILTER_MAX)), 0, 2,
                             Qt::AlignRight);
    volume_layout->addWidget(filter_slider, 1, 0, 1, 3);
    config_layout->addLayout(volume_layout);
    config_layout->setStretch(1, 3);

    m_mic_device_comboboxes[index] = new QComboBox();
#ifndef HAVE_CUBEB
    m_combobox_microphone[index]->addItem(
        QLatin1String("(%1)").arg(tr("Audio backend unsupported")), QString{});
#else
    m_mic_device_comboboxes[index]->addItem(
        QLatin1String("(%1)").arg(tr("Autodetect preferred microphone")), QString{});
    for (auto& [device_id, device_name] : CubebUtils::ListInputDevices())
    {
      const auto user_data = QString::fromStdString(device_id);
      m_mic_device_comboboxes[index]->addItem(QString::fromStdString(device_name), user_data);
    }
#endif
    auto current_device_id =
        QString::fromStdString(Config::Get(Config::MAIN_LOGITECH_MIC_MICROPHONE[index]));
    m_mic_device_comboboxes[index]->setCurrentIndex(
        m_mic_device_comboboxes[index]->findData(current_device_id));
    connect(m_mic_device_comboboxes[index], &QComboBox::currentIndexChanged, this,
            [index, this]() { OnInputDeviceChange(index); });
    config_layout->addWidget(m_mic_device_comboboxes[index]);

    config_group->setLayout(config_layout);
    main_config_layout->addWidget(config_group);
  }

  main_config_group->setLayout(main_config_layout);
  main_layout->addWidget(main_config_group);
}

void LogitechMicWindow::OnEmulationStateChanged(Core::State state)
{
  const bool running = state != Core::State::Uninitialized;

  for (std::size_t index = 0; index != Config::EMULATED_LOGITECH_MIC_COUNT; ++index)
  {
    m_mic_enabled_checkboxes[index]->setEnabled(!running);
  }
}

void LogitechMicWindow::OnInputDeviceChange(std::size_t index)
{
  auto user_data = m_mic_device_comboboxes[index]->currentData();
  if (user_data.isValid())
  {
    const std::string device_id = user_data.toString().toStdString();
    Config::SetBaseOrCurrent(Config::MAIN_LOGITECH_MIC_MICROPHONE[index], device_id);
  }
}
