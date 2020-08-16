// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/HotkeyPrimeHack.h"

#include <QGridLayout>
#include <QGroupBox>

#include "Core/HotkeyManager.h"

HotkeyPrimeHack::HotkeyPrimeHack(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyPrimeHack::CreateMainLayout()
{
  m_main_layout = new QGridLayout;

  m_main_layout->addWidget(
      CreateGroupBox(tr("Cheats"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_PRIMEHACK_CHEATS)), 0, 0);

  m_main_layout->addWidget(
    CreateGroupBox(tr("Graphics"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_PRIMEHACK_GFX)), 1, 0);

  setLayout(m_main_layout);
}

InputConfig* HotkeyPrimeHack::GetConfig()
{
  return HotkeyManagerEmu::GetConfig();
}

void HotkeyPrimeHack::LoadSettings()
{
  HotkeyManagerEmu::LoadConfig();
}

void HotkeyPrimeHack::SaveSettings()
{
  HotkeyManagerEmu::GetConfig()->SaveConfig();
}
