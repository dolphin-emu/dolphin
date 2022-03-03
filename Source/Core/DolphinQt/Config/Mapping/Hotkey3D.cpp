// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/Hotkey3D.h"

#include <QGroupBox>
#include <QHBoxLayout>

#include "Core/HotkeyManager.h"

Hotkey3D::Hotkey3D(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void Hotkey3D::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout();

  m_main_layout->addWidget(
      // i18n: Stereoscopic 3D
      CreateGroupBox(tr("3D"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_3D_TOGGLE)));
  m_main_layout->addWidget(
      // i18n: Stereoscopic 3D
      CreateGroupBox(tr("3D Depth"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_3D_DEPTH)));

  setLayout(m_main_layout);
}

InputConfig* Hotkey3D::GetConfig()
{
  return HotkeyManagerEmu::GetConfig();
}

void Hotkey3D::LoadSettings()
{
  HotkeyManagerEmu::LoadConfig();
}

void Hotkey3D::SaveSettings()
{
  HotkeyManagerEmu::GetConfig()->SaveConfig();
}
