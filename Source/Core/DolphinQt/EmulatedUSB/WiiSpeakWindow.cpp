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

#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/System.h"
#include "DolphinQt/QtUtils/CubebUtils.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

WiiSpeakWindow::WiiSpeakWindow(QWidget* parent) : QWidget(parent)
{
  // i18n: Window for managing the Wii Speak microphone
  setWindowTitle(tr("Wii Speak Manager"));
  setWindowIcon(Resources::GetAppIcon());
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
  checkbox_mic_muted->setChecked(Settings::Instance().IsWiiSpeakMuted());
  connect(&Settings::Instance(), &Settings::WiiSpeakMuteChanged, checkbox_mic_muted,
          &QCheckBox::setChecked);
  connect(checkbox_mic_muted, &QCheckBox::toggled, &Settings::Instance(),
          &Settings::SetWiiSpeakMuted);
  checkbox_mic_muted->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
  config_layout->addWidget(checkbox_mic_muted);

  config_layout->addLayout(
      QtUtils::CubebVolumeLayout(Config::MAIN_WII_SPEAK_VOLUME_MODIFIER, this));
  config_layout->setStretch(1, 3);

  m_combobox_microphones =
      QtUtils::CubebMicrophonesComboBox(Config::MAIN_WII_SPEAK_MICROPHONE, this);
  config_layout->addWidget(m_combobox_microphones);

  config_group->setLayout(config_layout);
  main_layout->addWidget(config_group);

  setLayout(main_layout);
}

void WiiSpeakWindow::EmulateWiiSpeak(bool emulate)
{
  Config::SetBaseOrCurrent(Config::MAIN_EMULATE_WII_SPEAK, emulate);
}

void WiiSpeakWindow::OnEmulationStateChanged(Core::State state)
{
  const bool running = state != Core::State::Uninitialized;

  m_checkbox_enabled->setEnabled(!running);
  m_combobox_microphones->setEnabled(!running);
}
