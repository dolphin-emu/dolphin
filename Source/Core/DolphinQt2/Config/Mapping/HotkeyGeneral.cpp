// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/Mapping/HotkeyGeneral.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "Core/HotkeyManager.h"

HotkeyGeneral::HotkeyGeneral(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyGeneral::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout();

  m_main_layout->addWidget(
      CreateGroupBox(tr("General"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_GENERAL)));

  auto* vbox = new QVBoxLayout();
  vbox->addWidget(CreateGroupBox(tr("Volume"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_VOLUME)));
  vbox->addWidget(
      CreateGroupBox(tr("Emulation Speed"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_SPEED)));
  m_main_layout->addItem(vbox);

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
