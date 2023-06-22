// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/GCMicrophone.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "InputCommon/InputConfig.h"

#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"

GCMicrophone::GCMicrophone(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void GCMicrophone::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout();

  m_main_layout->addWidget(
      CreateGroupBox(tr("Microphone"), Pad::GetGroup(GetPort(), PadGroup::Mic)));

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
