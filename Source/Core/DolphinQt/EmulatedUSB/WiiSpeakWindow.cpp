// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/EmulatedUSB/WiiSpeakWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QString>
#include <QVBoxLayout>

#include "AudioCommon/CubebUtils.h"
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

  installEventFilter(this);

  OnEmulationStateChanged(Core::GetState(Core::System::GetInstance()));
};

WiiSpeakWindow::~WiiSpeakWindow() = default;

void WiiSpeakWindow::CreateMainWindow()
{
  auto* main_layout = new QVBoxLayout();

  auto* checkbox_group = new QGroupBox();
  auto* checkbox_layout = new QHBoxLayout();
  checkbox_layout->setAlignment(Qt::AlignHCenter);
  m_checkbox_enabled = new QCheckBox(tr("Emulate Wii Speak"), this);
  m_checkbox_enabled->setChecked(Config::Get(Config::MAIN_EMULATE_WII_SPEAK));
  connect(m_checkbox_enabled, &QCheckBox::toggled, this, &WiiSpeakWindow::EmulateWiiSpeak);
  checkbox_layout->addWidget(m_checkbox_enabled);
  checkbox_group->setLayout(checkbox_layout);
  main_layout->addWidget(checkbox_group);

  m_config_group = new QGroupBox(tr("Microphone Configuration"));
  auto* config_layout = new QHBoxLayout();

  auto checkbox_mic_muted = new QCheckBox(tr("Mute"), this);
  checkbox_mic_muted->setChecked(Config::Get(Config::MAIN_WII_SPEAK_MUTED));
  connect(checkbox_mic_muted, &QCheckBox::toggled, this,
          &WiiSpeakWindow::SetWiiSpeakConnectionState);
  config_layout->addWidget(checkbox_mic_muted);

  m_combobox_microphones = new QComboBox();
  m_combobox_microphones->addItem(QLatin1String("(%1)").arg(tr("Autodetect preferred microphone")),
                                  QString{});
  for (auto& [device_id, device_name] : CubebUtils::ListInputDevices())
  {
    const auto user_data = QString::fromStdString(device_id);
    m_combobox_microphones->addItem(QString::fromStdString(device_name), user_data);
  }
  connect(m_combobox_microphones, &QComboBox::currentIndexChanged, this,
          &WiiSpeakWindow::OnInputDeviceChange);

  auto current_device_id = QString::fromStdString(Config::Get(Config::MAIN_WII_SPEAK_MICROPHONE));
  m_combobox_microphones->setCurrentIndex(m_combobox_microphones->findData(current_device_id));
  config_layout->addWidget(m_combobox_microphones);

  m_config_group->setLayout(config_layout);
  m_config_group->setVisible(Config::Get(Config::MAIN_EMULATE_WII_SPEAK));
  main_layout->addWidget(m_config_group);

  setLayout(main_layout);
}

void WiiSpeakWindow::EmulateWiiSpeak(bool emulate)
{
  Config::SetBaseOrCurrent(Config::MAIN_EMULATE_WII_SPEAK, emulate);
  m_config_group->setVisible(emulate);
}

void WiiSpeakWindow::SetWiiSpeakConnectionState(bool muted)
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
