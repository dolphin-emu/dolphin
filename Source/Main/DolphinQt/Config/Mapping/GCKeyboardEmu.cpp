// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "DolphinQt/Config/Mapping/GCKeyboardEmu.h"

#include "InputCommon/InputConfig.h"

#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCKeyboardEmu.h"

GCKeyboardEmu::GCKeyboardEmu(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void GCKeyboardEmu::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout();

  m_main_layout->addWidget(
      CreateGroupBox(QStringLiteral(""), Keyboard::GetGroup(GetPort(), KeyboardGroup::Kb0x)));
  m_main_layout->addWidget(
      CreateGroupBox(QStringLiteral(""), Keyboard::GetGroup(GetPort(), KeyboardGroup::Kb1x)));
  m_main_layout->addWidget(
      CreateGroupBox(QStringLiteral(""), Keyboard::GetGroup(GetPort(), KeyboardGroup::Kb2x)));
  m_main_layout->addWidget(
      CreateGroupBox(QStringLiteral(""), Keyboard::GetGroup(GetPort(), KeyboardGroup::Kb3x)));
  m_main_layout->addWidget(
      CreateGroupBox(QStringLiteral(""), Keyboard::GetGroup(GetPort(), KeyboardGroup::Kb4x)));

  auto* vbox_layout = new QVBoxLayout();
  vbox_layout->addWidget(
      CreateGroupBox(QStringLiteral(""), Keyboard::GetGroup(GetPort(), KeyboardGroup::Kb5x)));

  m_main_layout->addLayout(vbox_layout);

  setLayout(m_main_layout);
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
