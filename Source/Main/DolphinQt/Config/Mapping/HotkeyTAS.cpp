// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/HotkeyTAS.h"

#include <QGroupBox>
#include <QHBoxLayout>

#include "Core/HotkeyManager.h"

HotkeyTAS::HotkeyTAS(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyTAS::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout();

  m_main_layout->addWidget(
      CreateGroupBox(tr("Frame Advance"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_FRAME_ADVANCE)));
  m_main_layout->addWidget(
      CreateGroupBox(tr("Movie"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_MOVIE)));

  setLayout(m_main_layout);
}

InputConfig* HotkeyTAS::GetConfig()
{
  return HotkeyManagerEmu::GetConfig();
}

void HotkeyTAS::LoadSettings()
{
  HotkeyManagerEmu::LoadConfig();
}

void HotkeyTAS::SaveSettings()
{
  HotkeyManagerEmu::GetConfig()->SaveConfig();
}
