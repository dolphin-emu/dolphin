// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/GCPadEmu.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"

#include "InputCommon/ControllerEmu/Setting/BooleanSetting.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/InputConfig.h"

GCPadEmu::GCPadEmu(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void GCPadEmu::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout();

  m_main_layout->addWidget(
      CreateGroupBox(tr("Buttons"), Pad::GetGroup(GetPort(), PadGroup::Buttons)));
  m_main_layout->addWidget(
      CreateGroupBox(tr("Control Stick"), Pad::GetGroup(GetPort(), PadGroup::MainStick)));
  m_main_layout->addWidget(
      CreateGroupBox(tr("C Stick"), Pad::GetGroup(GetPort(), PadGroup::CStick)));
  m_main_layout->addWidget(CreateGroupBox(tr("D-Pad"), Pad::GetGroup(GetPort(), PadGroup::DPad)));

  auto* hbox_layout = new QVBoxLayout();

  m_main_layout->addLayout(hbox_layout);

  hbox_layout->addWidget(
      CreateGroupBox(tr("Triggers"), Pad::GetGroup(GetPort(), PadGroup::Triggers)));
  hbox_layout->addWidget(CreateGroupBox(tr("Rumble"), Pad::GetGroup(GetPort(), PadGroup::Rumble)));

  hbox_layout->addWidget(
      CreateGroupBox(tr("Options"), Pad::GetGroup(GetPort(), PadGroup::Options)));

  setLayout(m_main_layout);
}

void GCPadEmu::LoadSettings()
{
  Pad::LoadConfig();
}

void GCPadEmu::SaveSettings()
{
  Pad::GetConfig()->SaveConfig();
}

InputConfig* GCPadEmu::GetConfig()
{
  return Pad::GetConfig();
}
