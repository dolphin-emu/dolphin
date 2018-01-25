// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "DolphinQt2/Config/Mapping/GCPadEmu.h"
#include "InputCommon/InputConfig.h"

#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"

GCPadEmu::GCPadEmu(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void GCPadEmu::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout();

  auto* hbox_layout = new QVBoxLayout();

  hbox_layout->addWidget(
      CreateGroupBox(tr("Triggers"), Pad::GetGroup(GetPort(), PadGroup::Triggers)));
  hbox_layout->addWidget(CreateGroupBox(tr("Rumble"), Pad::GetGroup(GetPort(), PadGroup::Rumble)));

  m_main_layout->addWidget(
      CreateGroupBox(tr("Buttons"), Pad::GetGroup(GetPort(), PadGroup::Buttons)));
  m_main_layout->addWidget(
      CreateGroupBox(tr("Control Stick"), Pad::GetGroup(GetPort(), PadGroup::MainStick)));
  m_main_layout->addWidget(
      CreateGroupBox(tr("C Stick"), Pad::GetGroup(GetPort(), PadGroup::CStick)));
  m_main_layout->addWidget(CreateGroupBox(tr("D-Pad"), Pad::GetGroup(GetPort(), PadGroup::DPad)));
  m_main_layout->addItem(hbox_layout);

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
