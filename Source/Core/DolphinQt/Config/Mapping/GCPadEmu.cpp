// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/GCPadEmu.h"

#include <QGridLayout>
#include <QGroupBox>

#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_DeviceAMBaseboard.h"

#include "Core/Config/MainSettings.h"

#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/InputConfig.h"

GCPadEmu::GCPadEmu(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void GCPadEmu::CreateMainLayout()
{
  auto* layout = new QGridLayout;

  layout->addWidget(CreateGroupBox(tr("Buttons"), Pad::GetGroup(GetPort(), PadGroup::Buttons)), 0,
                    0);
  layout->addWidget(CreateGroupBox(tr("D-Pad"), Pad::GetGroup(GetPort(), PadGroup::DPad)), 1, 0);

  if (Config::Get(Config::GetInfoForSIDevice(0)) ==
      SerialInterface::SIDevices::SIDEVICE_AM_BASEBOARD)
  {
    layout->addWidget(CreateGroupBox(tr("Triforce"), Pad::GetGroup(GetPort(), PadGroup::Triforce)),
                      2, 0);
  }

  layout->addWidget(
      CreateGroupBox(tr("Control Stick"), Pad::GetGroup(GetPort(), PadGroup::MainStick)), 0, 1, -1,
      1);
  layout->addWidget(CreateGroupBox(tr("C Stick"), Pad::GetGroup(GetPort(), PadGroup::CStick)), 0, 2,
                    -1, 1);
  layout->addWidget(CreateGroupBox(tr("Triggers"), Pad::GetGroup(GetPort(), PadGroup::Triggers)), 0,
                    4);
  layout->addWidget(CreateGroupBox(tr("Rumble"), Pad::GetGroup(GetPort(), PadGroup::Rumble)), 1, 4);

  layout->addWidget(CreateGroupBox(tr("Options"), Pad::GetGroup(GetPort(), PadGroup::Options)), 2,
                    4);

  setLayout(layout);
}

void GCPadEmu::LoadSettings()
{
  Pad::LoadConfig();
}

void GCPadEmu::SaveSettings()
{
  Pad::GetConfig()->SaveConfig();
}

InputConfig* GCPadEmu::GetConfig()
{
  return Pad::GetConfig();
}
