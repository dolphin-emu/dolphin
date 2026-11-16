// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/CubebUtils.h"

#include <algorithm>

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QString>

#ifdef HAVE_CUBEB
#include "AudioCommon/CubebUtils.h"
#endif
#include "DolphinQt/Config/ConfigControls/ConfigChoice.h"

namespace
{
void UpdateMicrophoneComboBoxDeviceList(ConfigStringChoice* combobox)
{
  std::vector<std::pair<QString, QString>> input_devices;
#ifndef HAVE_CUBEB
  input_devices.emplace_back(QLatin1String("(%1)").arg(QWidget::tr("Audio backend unsupported")),
                             QString{});
#else
  input_devices.emplace_back(
      QLatin1String("(%1)").arg(QWidget::tr("Autodetect preferred microphone")), QString{});

  for (const auto& [device_id, device_name] : CubebUtils::ListInputDevices())
  {
    const auto user_data = QString::fromStdString(device_id);
    input_devices.emplace_back(QString::fromStdString(device_name), user_data);
  }
#endif
  combobox->SetOptions(input_devices);
}
}  // namespace

QGridLayout* QtUtils::CubebVolumeLayout(const Config::Info<s16>& cfg_volume, QWidget* parent)
{
  static constexpr int FILTER_MIN = -50;
  static constexpr int FILTER_MAX = 50;
  auto* const volume_layout = new QGridLayout();

  const int volume_modifier = std::clamp<int>(Config::Get(cfg_volume), FILTER_MIN, FILTER_MAX);
  auto* const filter_slider = new QSlider(Qt::Horizontal);
  auto* const slider_label =
      new QLabel(parent->tr("Volume modifier (value: %1dB)").arg(volume_modifier));
  parent->connect(filter_slider, &QSlider::valueChanged, slider_label,
                  [slider_label, cfg_volume, parent](int value) {
                    Config::SetBaseOrCurrent(cfg_volume, value);
                    slider_label->setText(parent->tr("Volume modifier (value: %1dB)").arg(value));
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
  return volume_layout;
}

QComboBox* QtUtils::CubebMicrophonesComboBox(const Config::Info<std::string>& cfg_microphone,
                                             QWidget* parent)
{
  const std::array<std::string, 0> empty_list;
  auto* const combobox = new ConfigStringChoice{empty_list, cfg_microphone};
  combobox->setParent(parent);

  combobox->connect(&Settings::Instance(), &Settings::AudioBackendChanged, combobox,
                    [combobox] { UpdateMicrophoneComboBoxDeviceList(combobox); });
  UpdateMicrophoneComboBoxDeviceList(combobox);

  // TODO: It may make sense to update this list when physical devices are added/removed.
  // Maybe just update the combobox when it's clicked on ?

  return combobox;
}
