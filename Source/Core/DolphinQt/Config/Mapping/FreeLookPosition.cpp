// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/FreeLookPosition.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>

#include "Core/FreeLookManager.h"
#include "InputCommon/InputConfig.h"

FreeLookPosition::FreeLookPosition(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void FreeLookPosition::CreateMainLayout()
{
  m_main_layout = new QGridLayout;

  m_main_layout->addWidget(
      CreateGroupBox(tr("Move"), FreeLook::GetInputGroup(GetPort(), FreeLookGroup::Move)), 0, 0);

  m_main_layout->addWidget(
      CreateGroupBox(tr("Offset"),
                     FreeLook::GetInputGroup(GetPort(), FreeLookGroup::PositionOffset)),
      0, 1);

  m_main_layout->setColumnStretch(0, 1);
  m_main_layout->setColumnStretch(1, 1);

  setLayout(m_main_layout);
}

InputConfig* FreeLookPosition::GetConfig()
{
  return FreeLook::GetInputConfig();
}

void FreeLookPosition::LoadSettings()
{
  FreeLook::LoadInputConfig();
}

void FreeLookPosition::SaveSettings()
{
  FreeLook::GetInputConfig()->SaveConfig();
}
