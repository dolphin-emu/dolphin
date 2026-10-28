// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/EmulatedUSB/LogitechMicWindow.h"

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>

#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/System.h"
#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/QtUtils/CubebUtils.h"
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

    config_layout->addLayout(
        QtUtils::CubebVolumeLayout(Config::MAIN_LOGITECH_MIC_VOLUME_MODIFIER[index], this));
    config_layout->setStretch(1, 3);

    config_layout->addWidget(
        QtUtils::CubebMicrophonesComboBox(Config::MAIN_LOGITECH_MIC_MICROPHONE[index], this));

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
