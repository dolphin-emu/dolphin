// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/HotkeyGraphics.h"

#include <QGridLayout>
#include <QGroupBox>

#include "Core/HotkeyManager.h"

HotkeyGraphics::HotkeyGraphics(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyGraphics::CreateMainLayout()
{
  m_main_layout = new QGridLayout();

  m_main_layout->addWidget(CreateGroupBox(tr("Graphics Toggles"),
                                          HotkeyManagerEmu::GetHotkeyGroup(HKGP_GRAPHICS_TOGGLES)),
                           0, 0, -1, 1);

  m_main_layout->addWidget(
      CreateGroupBox(tr("FreeLook"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_FREELOOK)), 0, 1);
  m_main_layout->addWidget(
      CreateGroupBox(tr("Internal Resolution"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_IR)), 1, 1);

  setLayout(m_main_layout);
}

InputConfig* HotkeyGraphics::GetConfig()
{
  return HotkeyManagerEmu::GetConfig();
}

void HotkeyGraphics::LoadSettings()
{
  HotkeyManagerEmu::LoadConfig();
}

void HotkeyGraphics::SaveSettings()
{
  HotkeyManagerEmu::GetConfig()->SaveConfig();
}
