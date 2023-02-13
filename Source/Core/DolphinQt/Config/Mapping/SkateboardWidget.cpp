// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/SkateboardWidget.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QGroupBox>

#include "Common/Config/Config.h"
#include "Core/Config/MainSettings.h"
#include "Core/IOS/USB/Emulated/Skateboard.h"
#include "InputCommon/InputConfig.h"

SkateboardWidget::SkateboardWidget(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void SkateboardWidget::CreateMainLayout()
{
  auto* layout = new QVBoxLayout;

  using namespace Skateboard;
  const int port = GetPort();

  auto* checkbox = new QCheckBox(tr("Emulate Skateboard"));
  connect(checkbox, &QCheckBox::toggled,
          [](bool checked) { Config::SetBaseOrCurrent(Config::MAIN_EMULATE_SKATEBOARD, checked); });

  auto* buttons = new QVBoxLayout;
  buttons->addWidget(CreateGroupBox(GetGroup(port, Group::ButtonsAB12)));
  buttons->addWidget(CreateGroupBox(GetGroup(port, Group::ButtonsPlusMinusPower)));
  buttons->addWidget(CreateGroupBox(GetGroup(port, Group::ButtonsDirectional)));

  auto* inputs = new QHBoxLayout;
  inputs->addLayout(buttons);
  inputs->addWidget(CreateGroupBox(tr("IR Sensors"), GetGroup(port, Group::IRSensors)));
  inputs->addWidget(CreateGroupBox(GetGroup(port, Group::AccelNose)));
  inputs->addWidget(CreateGroupBox(GetGroup(port, Group::AccelTail)));

  layout->addWidget(checkbox);
  layout->addLayout(inputs);
  setLayout(layout);
}

void SkateboardWidget::LoadSettings()
{
  Skateboard::GetConfig()->LoadConfig();
}

void SkateboardWidget::SaveSettings()
{
  Skateboard::GetConfig()->SaveConfig();
}

InputConfig* SkateboardWidget::GetConfig()
{
  return Skateboard::GetConfig();
}
