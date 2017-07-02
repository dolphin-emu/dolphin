// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/Mapping/HotkeyGraphics.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "Core/HotkeyManager.h"

HotkeyGraphics::HotkeyGraphics(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void HotkeyGraphics::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout();

  m_main_layout->addWidget(
      CreateGroupBox(tr("Freelook"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_FREELOOK)));

  auto* vbox = new QVBoxLayout();
  vbox->addWidget(CreateGroupBox(tr("Graphics Toggles"),
                                 HotkeyManagerEmu::GetHotkeyGroup(HKGP_GRAPHICS_TOGGLES)));
  vbox->addWidget(
      CreateGroupBox(tr("Internal Resolution"), HotkeyManagerEmu::GetHotkeyGroup(HKGP_IR)));
  m_main_layout->addItem(vbox);

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
