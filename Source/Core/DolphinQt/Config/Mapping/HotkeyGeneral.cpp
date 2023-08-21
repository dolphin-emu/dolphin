// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/HotkeyGeneral.h"

#include <QGridLayout>
#include <QGroupBox>

#include "Core/HotkeyManager.h"

HotkeyGeneral::HotkeyGeneral(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyGeneral::CreateMainLayout()
{
  m_main_layout = new QGridLayout;

  m_main_layout->addWidget(
      CreateGroupBox(tr("General"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_GENERAL)), 0, 0, -1, 1);

  m_main_layout->addWidget(
      CreateGroupBox(tr("Volume"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_VOLUME)), 0, 1);
  m_main_layout->addWidget(
      CreateGroupBox(tr("Emulation Speed"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_SPEED)), 1, 1);

  setLayout(m_main_layout);
}

InputConfig* HotkeyGeneral::GetConfig()
{
  return HotkeyManagerEmu::GetConfig();
}

void HotkeyGeneral::LoadSettings()
{
  HotkeyManagerEmu::LoadConfig();
}

void HotkeyGeneral::SaveSettings()
{
  HotkeyManagerEmu::GetConfig()->SaveConfig();
}
