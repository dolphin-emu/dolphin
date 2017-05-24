// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "DolphinQt2/Config/Mapping/GCKeyboardEmu.h"
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
  auto* options_box =
      CreateGroupBox(tr("Options"), Keyboard::GetGroup(GetPort(), KeyboardGroup::Options));

  options_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  vbox_layout->addWidget(
      CreateGroupBox(QStringLiteral(""), Keyboard::GetGroup(GetPort(), KeyboardGroup::Kb5x)));
  vbox_layout->addWidget(options_box);

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
