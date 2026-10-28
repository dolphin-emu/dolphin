// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/GCMicrophone.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "InputCommon/InputConfig.h"

#include "Common/Assert.h"
#include "Core/Config/MainSettings.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/QtUtils/CubebUtils.h"

GCMicrophone::GCMicrophone(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void GCMicrophone::CreateMainLayout()
{
  m_main_layout = new QVBoxLayout();

  m_main_layout->addWidget(
      CreateGroupBox(tr("Microphone"), Pad::GetGroup(GetPort(), PadGroup::Mic)));

  const int index = GetPort();
  auto* const config_group = new QGroupBox(tr("Microphone %1 Configuration").arg(index + 1));
  auto* const config_layout = new QHBoxLayout();
  ASSERT_MSG(CORE, index == 0 || index == 1, "Invalid GameCube microphone port {}", index);

  auto* const mic_muted = new ConfigBool(tr("Mute"), Config::MAIN_EXI_MIC_MUTED[index]);
  mic_muted->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
  config_layout->addWidget(mic_muted);

  config_layout->addLayout(
      QtUtils::CubebVolumeLayout(Config::MAIN_EXI_MIC_VOLUME_MODIFIER[index], this));
  config_layout->setStretch(1, 3);

  config_layout->addWidget(
      QtUtils::CubebMicrophonesComboBox(Config::MAIN_EXI_MIC_MICROPHONE[index], this));

  config_group->setLayout(config_layout);
  m_main_layout->addWidget(config_group);

  setLayout(m_main_layout);
}

void GCMicrophone::LoadSettings()
{
  Pad::LoadConfig();
}

void GCMicrophone::SaveSettings()
{
  Pad::GetConfig()->SaveConfig();
}

InputConfig* GCMicrophone::GetConfig()
{
  return Pad::GetConfig();
}
