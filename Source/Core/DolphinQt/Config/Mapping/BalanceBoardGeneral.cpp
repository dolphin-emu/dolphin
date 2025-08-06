// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/BalanceBoardGeneral.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "DolphinQt/Config/Mapping/MappingWindow.h"

#include "InputCommon/InputConfig.h"

BalanceBoardGeneral::BalanceBoardGeneral(MappingWindow* window) : MappingWidget(window)
{
  auto* layout = new QHBoxLayout;

  auto& get_group = BalanceBoard::GetBalanceBoardGroup;
  using BBG = WiimoteEmu::BalanceBoardGroup;

  layout->addWidget(CreateGroupBox(tr("Buttons"), get_group(GetPort(), BBG::Buttons)));
  layout->addWidget(CreateGroupBox(tr("Balance"), get_group(GetPort(), BBG::Balance)));
  layout->addWidget(CreateGroupBox(tr("Options"), get_group(GetPort(), BBG::Options)));
  layout->addWidget(CreateGroupBox(tr("Left Foot"), get_group(GetPort(), BBG::LeftFoot)));
  layout->addWidget(CreateGroupBox(tr("Right Foot"), get_group(GetPort(), BBG::RightFoot)));

  setLayout(layout);
}

void BalanceBoardGeneral::LoadSettings()
{
  BalanceBoard::LoadConfig();
}

void BalanceBoardGeneral::SaveSettings()
{
  BalanceBoard::GetConfig()->SaveConfig();
}

InputConfig* BalanceBoardGeneral::GetConfig()
{
  return BalanceBoard::GetConfig();
}
