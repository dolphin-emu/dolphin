// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/WiimoteEmuExtensionMotionSimulation.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>

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
  auto* layout = new QGridLayout();
  m_nunchuk_box = new QGroupBox(tr("Nunchuk"), this);

  layout->addWidget(CreateGroupBox(tr("Shake"), Wiimote::GetNunchukGroup(
                                                    GetPort(), WiimoteEmu::NunchukGroup::Shake)),
                    0, 0);
  layout->addWidget(CreateGroupBox(tr("Tilt"), Wiimote::GetNunchukGroup(
                                                   GetPort(), WiimoteEmu::NunchukGroup::Tilt)),
                    0, 1);
  layout->addWidget(CreateGroupBox(tr("Swing"), Wiimote::GetNunchukGroup(
                                                    GetPort(), WiimoteEmu::NunchukGroup::Swing)),
                    0, 2);

  m_nunchuk_box->setLayout(layout);
}

void WiimoteEmuExtensionMotionSimulation::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout();

  m_main_layout->addWidget(m_nunchuk_box);

  setLayout(m_main_layout);
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
