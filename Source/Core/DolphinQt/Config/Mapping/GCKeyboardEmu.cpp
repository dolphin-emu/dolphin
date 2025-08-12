// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/GCKeyboardEmu.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QVBoxLayout>

#include "InputCommon/InputConfig.h"

#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCKeyboardEmu.h"

#include "DolphinQt/QtUtils/QtUtils.h"

GCKeyboardEmu::GCKeyboardEmu(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void GCKeyboardEmu::CreateMainLayout()
{
  auto* const vbox_layout = new QVBoxLayout{this};

  auto* const warning_text =
      new QLabel(tr("You are configuring a \"Keyboard Controller\". "
                    "This device is exclusively for \"Phantasy Star Online Episode I & II\". "
                    "If you are unsure, turn back now and configure a \"Standard Controller\"."));
  warning_text->setWordWrap(true);

  vbox_layout->addWidget(
      QtUtils::CreateIconWarning(this, QStyle::SP_MessageBoxWarning, warning_text));

  auto* const layout = new QHBoxLayout;

  using KG = KeyboardGroup;
  for (auto kbg : {KG::Kb0x, KG::Kb1x, KG::Kb2x, KG::Kb3x, KG::Kb4x, KG::Kb5x})
    layout->addWidget(CreateGroupBox(QString{}, Keyboard::GetGroup(GetPort(), kbg)));

  vbox_layout->addLayout(layout);
}

void GCKeyboardEmu::LoadSettings()
{
  Keyboard::LoadConfig();
}

void GCKeyboardEmu::SaveSettings()
{
  Keyboard::GetConfig()->SaveConfig();
}

InputConfig* GCKeyboardEmu::GetConfig()
{
  return Keyboard::GetConfig();
}
