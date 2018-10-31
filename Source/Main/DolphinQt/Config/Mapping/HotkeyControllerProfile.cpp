// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/HotkeyControllerProfile.h"

#include <QGroupBox>
#include <QHBoxLayout>

#include "Core/HotkeyManager.h"

HotkeyControllerProfile::HotkeyControllerProfile(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyControllerProfile::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout();

  m_main_layout->addWidget(CreateGroupBox(
      tr("Controller Profile"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_CONTROLLER_PROFILE)));

  setLayout(m_main_layout);
}

InputConfig* HotkeyControllerProfile::GetConfig()
{
  return HotkeyManagerEmu::GetConfig();
}

void HotkeyControllerProfile::LoadSettings()
{
  HotkeyManagerEmu::LoadConfig();
}

void HotkeyControllerProfile::SaveSettings()
{
  HotkeyManagerEmu::GetConfig()->SaveConfig();
}
