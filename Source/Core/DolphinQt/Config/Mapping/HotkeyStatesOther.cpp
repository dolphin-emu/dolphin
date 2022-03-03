// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/HotkeyStatesOther.h"

#include <QGroupBox>
#include <QHBoxLayout>

#include "Core/HotkeyManager.h"

HotkeyStatesOther::HotkeyStatesOther(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyStatesOther::CreateMainLayout()
{
  auto* layout = new QHBoxLayout;

  layout->addWidget(
      CreateGroupBox(tr("Select Last State"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_SELECT_STATE)));
  layout->addWidget(CreateGroupBox(tr("Load Last State"),
                                   HotkeyManagerEmu::GetHotkeyGroup(HKGP_LOAD_LAST_STATE)));
  layout->addWidget(
      CreateGroupBox(tr("Other State Hotkeys"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_STATE_MISC)));

  setLayout(layout);
}

InputConfig* HotkeyStatesOther::GetConfig()
{
  return HotkeyManagerEmu::GetConfig();
}

void HotkeyStatesOther::LoadSettings()
{
  HotkeyManagerEmu::LoadConfig();
}

void HotkeyStatesOther::SaveSettings()
{
  HotkeyManagerEmu::GetConfig()->SaveConfig();
}
