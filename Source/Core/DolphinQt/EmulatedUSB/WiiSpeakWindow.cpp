// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/EmulatedUSB/WiiSpeakWindow.h"

#include <QCheckBox>
#include <QComboBox>
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

  auto checkbox_mic_connected = new QCheckBox(tr("Connect"), this);
  checkbox_mic_connected->setChecked(Config::Get(Config::MAIN_WII_SPEAK_CONNECTED));
  connect(checkbox_mic_connected, &QCheckBox::toggled, this,
          &WiiSpeakWindow::SetWiiSpeakConnectionState);
  config_layout->addWidget(checkbox_mic_connected);

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

void WiiSpeakWindow::SetWiiSpeakConnectionState(bool connected)
{
  Config::SetBaseOrCurrent(Config::MAIN_WII_SPEAK_CONNECTED, connected);
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
