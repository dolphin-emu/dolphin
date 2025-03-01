// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/WiimoteEmuExtensionMotionSimulation.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "InputCommon/InputConfig.h"

WiimoteEmuExtensionMotionSimulation::WiimoteEmuExtensionMotionSimulation(MappingWindow* window)
    : MappingWidget(window)
{
  CreateNunchukLayout();
  CreateMainLayout();
}

void WiimoteEmuExtensionMotionSimulation::CreateNunchukLayout()
{
  m_nunchuk_box = new QGroupBox(tr("Nunchuk"), this);
  auto* const layout = new QHBoxLayout{m_nunchuk_box};

  layout->addWidget(
      CreateGroupBox(Wiimote::GetNunchukGroup(GetPort(), WiimoteEmu::NunchukGroup::Shake)));
  layout->addWidget(
      CreateGroupBox(Wiimote::GetNunchukGroup(GetPort(), WiimoteEmu::NunchukGroup::Tilt)));
  layout->addWidget(
      CreateGroupBox(Wiimote::GetNunchukGroup(GetPort(), WiimoteEmu::NunchukGroup::Swing)));
}

void WiimoteEmuExtensionMotionSimulation::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout{this};
  m_main_layout->addWidget(m_nunchuk_box);
}

void WiimoteEmuExtensionMotionSimulation::LoadSettings()
{
  Wiimote::LoadConfig();
}

void WiimoteEmuExtensionMotionSimulation::SaveSettings()
{
  Wiimote::GetConfig()->SaveConfig();
}

InputConfig* WiimoteEmuExtensionMotionSimulation::GetConfig()
{
  return Wiimote::GetConfig();
}
