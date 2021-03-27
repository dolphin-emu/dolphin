// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
  CreateMainLayout();
}

void BalanceBoardGeneral::CreateMainLayout()
{
  auto* layout = new QGridLayout;

  layout->addWidget(
      CreateGroupBox(tr("Buttons"), Wiimote::GetBalanceBoardGroup(
                                        GetPort(), WiimoteEmu::BalanceBoardGroup::Buttons)),
      0, 0);
  layout->addWidget(
      CreateGroupBox(tr("Weight"), Wiimote::GetBalanceBoardGroup(
                                       GetPort(), WiimoteEmu::BalanceBoardGroup::Weight)),
      0, 1);
  layout->addWidget(
      CreateGroupBox(tr("Balance"), Wiimote::GetBalanceBoardGroup(
                                        GetPort(), WiimoteEmu::BalanceBoardGroup::Balance)),
      0, 2);

  layout->addWidget(
      CreateGroupBox(tr("Options"), Wiimote::GetBalanceBoardGroup(
                                        GetPort(), WiimoteEmu::BalanceBoardGroup::Options)),
      0, 3);

  setLayout(layout);
}

void BalanceBoardGeneral::LoadSettings()
{
  Wiimote::LoadConfig();
}

void BalanceBoardGeneral::SaveSettings()
{
  Wiimote::GetConfig()->SaveConfig();
}

InputConfig* BalanceBoardGeneral::GetConfig()
{
  return Wiimote::GetConfig();
}
