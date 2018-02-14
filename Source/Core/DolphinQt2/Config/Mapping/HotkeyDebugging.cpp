// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/Mapping/HotkeyDebugging.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "Core/HotkeyManager.h"

HotkeyDebugging::HotkeyDebugging(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyDebugging::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout();

  m_main_layout->addWidget(
      CreateGroupBox(tr("Stepping"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_STEPPING)));

  auto* vbox = new QVBoxLayout();
  vbox->addWidget(CreateGroupBox(tr("Program Counter"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_PC)));
  vbox->addWidget(
      CreateGroupBox(tr("Breakpoint"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_BREAKPOINT)));
  m_main_layout->addItem(vbox);

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
