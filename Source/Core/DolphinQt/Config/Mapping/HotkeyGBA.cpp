// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/HotkeyGBA.h"

#include <QGroupBox>
#include <QHBoxLayout>

#include "Core/HotkeyManager.h"

HotkeyGBA::HotkeyGBA(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyGBA::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout();

  m_main_layout->addWidget(
      CreateGroupBox(tr("Core"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_GBA_CORE)));
  m_main_layout->addWidget(
      CreateGroupBox(tr("Volume"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_GBA_VOLUME)));
  m_main_layout->addWidget(
      CreateGroupBox(tr("Window Size"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_GBA_SIZE)));

  setLayout(m_main_layout);
}

InputConfig* HotkeyGBA::GetConfig()
{
  return HotkeyManagerEmu::GetConfig();
}

void HotkeyGBA::LoadSettings()
{
  HotkeyManagerEmu::LoadConfig();
}

void HotkeyGBA::SaveSettings()
{
  HotkeyManagerEmu::GetConfig()->SaveConfig();
}
