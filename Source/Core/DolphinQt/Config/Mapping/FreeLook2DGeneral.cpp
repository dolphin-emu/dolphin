// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/FreeLook2DGeneral.h"

#include <QGridLayout>
#include <QGroupBox>

#include "Core/FreeLookManager.h"
#include "InputCommon/InputConfig.h"

FreeLook2DGeneral::FreeLook2DGeneral(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void FreeLook2DGeneral::CreateMainLayout()
{
  auto* layout = new QGridLayout;

  layout->addWidget(
      CreateGroupBox(tr("Move"), FreeLook::Get2DInputGroup(GetPort(), FreeLook2DGroup::Move)), 0,
      0);
  layout->addWidget(
      CreateGroupBox(tr("Speed"), FreeLook::Get2DInputGroup(GetPort(), FreeLook2DGroup::Speed)), 0,
      1);
  layout->addWidget(
      CreateGroupBox(tr("Stretch"), FreeLook::Get2DInputGroup(GetPort(), FreeLook2DGroup::Stretch)),
      0, 2);
  layout->addWidget(
      CreateGroupBox(tr("Other"), FreeLook::Get2DInputGroup(GetPort(), FreeLook2DGroup::Other)), 0,
      3);

  setLayout(layout);
}

void FreeLook2DGeneral::LoadSettings()
{
  FreeLook::Load2DInputConfig();
}

void FreeLook2DGeneral::SaveSettings()
{
  FreeLook::Get2DInputConfig()->SaveConfig();
}

InputConfig* FreeLook2DGeneral::GetConfig()
{
  return FreeLook::Get2DInputConfig();
}
