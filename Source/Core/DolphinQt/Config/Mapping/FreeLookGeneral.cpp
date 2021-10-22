// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/FreeLookGeneral.h"

#include <QGridLayout>
#include <QGroupBox>

#include "Core/FreeLookManager.h"
#include "InputCommon/InputConfig.h"

FreeLookGeneral::FreeLookGeneral(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void FreeLookGeneral::CreateMainLayout()
{
  auto* layout = new QGridLayout;

  layout->addWidget(
      CreateGroupBox(tr("Move"), FreeLook::GetInputGroup(GetPort(), FreeLookGroup::Move)), 0, 0);
  layout->addWidget(
      CreateGroupBox(tr("Speed"), FreeLook::GetInputGroup(GetPort(), FreeLookGroup::Speed)), 0, 1);
  layout->addWidget(CreateGroupBox(tr("Field of View"),
                                   FreeLook::GetInputGroup(GetPort(), FreeLookGroup::FieldOfView)),
                    0, 2);
  layout->addWidget(
      CreateGroupBox(tr("Other"), FreeLook::GetInputGroup(GetPort(), FreeLookGroup::Other)), 0, 3);

  setLayout(layout);
}

void FreeLookGeneral::LoadSettings()
{
  FreeLook::LoadInputConfig();
}

void FreeLookGeneral::SaveSettings()
{
  FreeLook::GetInputConfig()->SaveConfig();
}

InputConfig* FreeLookGeneral::GetConfig()
{
  return FreeLook::GetInputConfig();
}
