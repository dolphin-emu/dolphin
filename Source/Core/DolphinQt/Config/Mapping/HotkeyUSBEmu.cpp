// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/HotkeyUSBEmu.h"

#include <QGroupBox>
#include <QHBoxLayout>

#include "Core/HotkeyManager.h"

HotkeyUSBEmu::HotkeyUSBEmu(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyUSBEmu::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout();

  m_main_layout->addWidget(
      CreateGroupBox(tr("USB Device Emulation"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_USB_EMU)));

  setLayout(m_main_layout);
}

InputConfig* HotkeyUSBEmu::GetConfig()
{
  return HotkeyManagerEmu::GetConfig();
}

void HotkeyUSBEmu::LoadSettings()
{
  HotkeyManagerEmu::LoadConfig();
}

void HotkeyUSBEmu::SaveSettings()
{
  HotkeyManagerEmu::GetConfig()->SaveConfig();
}
