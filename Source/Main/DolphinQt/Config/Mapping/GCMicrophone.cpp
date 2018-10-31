// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "DolphinQt/Config/Mapping/GCMicrophone.h"

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
