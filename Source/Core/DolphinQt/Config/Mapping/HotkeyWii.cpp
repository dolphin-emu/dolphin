// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/HotkeyWii.h"

#include <QGroupBox>
#include <QHBoxLayout>

#include "Core/HotkeyManager.h"

HotkeyWii::HotkeyWii(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyWii::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout();

  m_main_layout->addWidget(CreateGroupBox(tr("Wii"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_WII)));

  setLayout(m_main_layout);
}

InputConfig* HotkeyWii::GetConfig()
{
  return HotkeyManagerEmu::GetConfig();
}

void HotkeyWii::LoadSettings()
{
  HotkeyManagerEmu::LoadConfig();
}

void HotkeyWii::SaveSettings()
{
  HotkeyManagerEmu::GetConfig()->SaveConfig();
}
