// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/GBAPadEmu.h"

#include <QGridLayout>
#include <QGroupBox>

#include "Core/HW/GBAPad.h"
#include "Core/HW/GBAPadEmu.h"
#include "InputCommon/InputConfig.h"

GBAPadEmu::GBAPadEmu(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void GBAPadEmu::CreateMainLayout()
{
  auto* layout = new QGridLayout;

  layout->addWidget(
      CreateControlsBox(tr("D-Pad"), Pad::GetGBAGroup(GetPort(), GBAPadGroup::DPad), 2), 0, 0, -1,
      1);
  layout->addWidget(
      CreateControlsBox(tr("Buttons"), Pad::GetGBAGroup(GetPort(), GBAPadGroup::Buttons), 2), 0, 1,
      -1, 1);

  setLayout(layout);
}

void GBAPadEmu::LoadSettings()
{
  Pad::LoadGBAConfig();
}

void GBAPadEmu::SaveSettings()
{
  Pad::GetGBAConfig()->SaveConfig();
}

InputConfig* GBAPadEmu::GetConfig()
{
  return Pad::GetGBAConfig();
}
