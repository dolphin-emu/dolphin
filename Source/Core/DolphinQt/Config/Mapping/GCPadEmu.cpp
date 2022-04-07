// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/GCPadEmu.h"

#include <string>

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>

#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"

#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/DInput/DInputKeyboardMouse.h"
#include "InputCommon/InputConfig.h"

GCPadEmu::GCPadEmu(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void GCPadEmu::CreateMainLayout()
{
  auto* layout = new QGridLayout;

  layout->addWidget(CreateGroupBox(tr("Buttons"), Pad::GetGroup(GetPort(), PadGroup::Buttons)), 0,
                    0);
  layout->addWidget(CreateGroupBox(tr("D-Pad"), Pad::GetGroup(GetPort(), PadGroup::DPad)), 1, 0, -1,
                    1);
  layout->addWidget(
      CreateGroupBox(tr("Control Stick"), Pad::GetGroup(GetPort(), PadGroup::MainStick)), 0, 1, 1,
      1);
  layout->addWidget(CreateGroupBox(tr("C Stick"), Pad::GetGroup(GetPort(), PadGroup::CStick)), 0, 2,
                    -1, 1);
  layout->addWidget(CreateGroupBox(tr("Triggers"), Pad::GetGroup(GetPort(), PadGroup::Triggers)), 0,
                    4);
  layout->addWidget(CreateGroupBox(tr("Rumble"), Pad::GetGroup(GetPort(), PadGroup::Rumble)), 1, 4);

  layout->addWidget(CreateGroupBox(tr("Options"), Pad::GetGroup(GetPort(), PadGroup::Options)), 2,
                    4);

  //Keyboard and Mouse Settings
  QGroupBox* keyboard_and_mouse_box = new QGroupBox{tr("Keyboard and Mouse")};
  QVBoxLayout* keyboard_and_mouse_layout = new QVBoxLayout{};

  QHBoxLayout* sensitivity_layout = new QHBoxLayout{};
  QDoubleSpinBox* sensitivity_spin_box = new QDoubleSpinBox{};
  sensitivity_spin_box->setRange(0.00, 100.00);
  sensitivity_spin_box->setDecimals(2);
  sensitivity_spin_box->setSingleStep(1);
  QLabel* sensitivity_label = new QLabel{};
  sensitivity_label->setText(tr("Sensitivity"));
  sensitivity_layout->addWidget(sensitivity_label);
  sensitivity_layout->addWidget(sensitivity_spin_box);

  QHBoxLayout* snapping_distance_layout = new QHBoxLayout{};
  QDoubleSpinBox* snapping_distance_spin_box = new QDoubleSpinBox{};
  snapping_distance_spin_box->setRange(0.00, 100.00);
  snapping_distance_spin_box->setDecimals(2);
  snapping_distance_spin_box->setSingleStep(1);
  QLabel* snapping_distance_label = new QLabel{};
  snapping_distance_label->setText(tr("Snapping Distance"));
  snapping_distance_layout->addWidget(snapping_distance_label);
  snapping_distance_layout->addWidget(snapping_distance_spin_box);

  QHBoxLayout* center_mouse_key_layout = new QHBoxLayout{};
  QPushButton* center_mouse_key_button = new QPushButton{};
  center_mouse_key_button->setText(tr(std::string{(char)::ciface::DInput::center_mouse_key}.c_str()));
  QLabel* center_mouse_key_label = new QLabel{};
  center_mouse_key_label->setText(tr("Center Mouse Key"));
  center_mouse_key_layout->addWidget(center_mouse_key_label);
  center_mouse_key_layout->addWidget(center_mouse_key_button);

  //You were adding the callbacks to change the values from UI events

  keyboard_and_mouse_layout->addLayout(sensitivity_layout);
  keyboard_and_mouse_layout->addLayout(snapping_distance_layout);
  keyboard_and_mouse_layout->addLayout(center_mouse_key_layout);

  keyboard_and_mouse_box->setLayout(keyboard_and_mouse_layout);

  layout->addWidget(keyboard_and_mouse_box, 1, 1);
  //End Keyboard and Mouse Settings

  setLayout(layout);
}

void GCPadEmu::LoadSettings()
{
  Pad::LoadConfig();
}

void GCPadEmu::SaveSettings()
{
  Pad::GetConfig()->SaveConfig();
}

InputConfig* GCPadEmu::GetConfig()
{
  return Pad::GetConfig();
}
