// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/WiimoteEmuMotionControl.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "InputCommon/InputConfig.h"

WiimoteEmuMotionControl::WiimoteEmuMotionControl(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void WiimoteEmuMotionControl::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout();

  m_main_layout->addWidget(
      CreateGroupBox(Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Shake)));
  m_main_layout->addWidget(
      CreateGroupBox(Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Point)));
  m_main_layout->addWidget(
      CreateGroupBox(Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Tilt)));
  m_main_layout->addWidget(
      CreateGroupBox(Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Swing)));

  setLayout(m_main_layout);
}

void WiimoteEmuMotionControl::LoadSettings()
{
  Wiimote::LoadConfig();
}

void WiimoteEmuMotionControl::SaveSettings()
{
  Wiimote::GetConfig()->SaveConfig();
}

InputConfig* WiimoteEmuMotionControl::GetConfig()
{
  return Wiimote::GetConfig();
}
