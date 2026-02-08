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

GCKeyboardEmu::GCKeyboardEmu(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void GCKeyboardEmu::CreateMainLayout()
{
  const auto vbox_layout = new QVBoxLayout;

  const auto warning_layout = new QHBoxLayout;
  vbox_layout->addLayout(warning_layout);

  const auto warning_icon = new QLabel;
  const auto size = QFontMetrics(font()).height() * 3 / 2;
  warning_icon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(size, size));
  warning_layout->addWidget(warning_icon);

  const auto warning_text =
      new QLabel(tr("You are configuring a \"Keyboard Controller\". "
                    "This device is exclusively for \"Phantasy Star Online Episode I & II\". "
                    "If you are unsure, turn back now and configure a \"Standard Controller\"."));
  warning_text->setWordWrap(true);
  warning_layout->addWidget(warning_text, 1);

  const auto layout = new QHBoxLayout;

  using KG = KeyboardGroup;
  for (auto kbg : {KG::Kb0x, KG::Kb1x, KG::Kb2x, KG::Kb3x, KG::Kb4x, KG::Kb5x})
    layout->addWidget(CreateGroupBox(QString{}, Keyboard::GetGroup(GetPort(), kbg)));

  vbox_layout->addLayout(layout);

  setLayout(vbox_layout);
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
