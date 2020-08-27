// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/HotkeyDebugging.h"

#include <QGridLayout>
#include <QGroupBox>

#include "Core/HotkeyManager.h"

HotkeyDebugging::HotkeyDebugging(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyDebugging::CreateMainLayout()
{
  m_main_layout = new QGridLayout();

  m_main_layout->addWidget(
      CreateGroupBox(tr("Stepping"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_STEPPING)), 0, 0, -1, 1);

  m_main_layout->addWidget(
      CreateGroupBox(tr("Program Counter"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_PC)), 0, 1);
  m_main_layout->addWidget(
      CreateGroupBox(tr("Breakpoint"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_BREAKPOINT)), 1, 1);

  setLayout(m_main_layout);
}

InputConfig* HotkeyDebugging::GetConfig()
{
  return HotkeyManagerEmu::GetConfig();
}

void HotkeyDebugging::LoadSettings()
{
  HotkeyManagerEmu::LoadConfig();
}

void HotkeyDebugging::SaveSettings()
{
  HotkeyManagerEmu::GetConfig()->SaveConfig();
}
