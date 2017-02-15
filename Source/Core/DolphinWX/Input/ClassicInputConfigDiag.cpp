// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Input/ClassicInputConfigDiag.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

ClassicInputConfigDialog::ClassicInputConfigDialog(wxWindow* const parent, InputConfig& config,
                                                   const wxString& name,
                                                   wxComboBox* device_cbox_parent,
                                                   const int port_num)
    : InputConfigDialog(parent, config, name, port_num)
{
  const int space5 = FromDIP(5);

  device_cbox = device_cbox_parent;

  auto* const group_box_buttons = new ControlGroupBox(
      Wiimote::GetClassicGroup(port_num, WiimoteEmu::ClassicGroup::Buttons), this, this);
  auto* const group_box_dpad = new ControlGroupBox(
      Wiimote::GetClassicGroup(port_num, WiimoteEmu::ClassicGroup::DPad), this, this);
  auto* const group_box_left_stick = new ControlGroupBox(
      Wiimote::GetClassicGroup(port_num, WiimoteEmu::ClassicGroup::LeftStick), this, this);
  auto* const group_box_right_stick = new ControlGroupBox(
      Wiimote::GetClassicGroup(port_num, WiimoteEmu::ClassicGroup::RightStick), this, this);
  auto* const group_box_triggers = new ControlGroupBox(
      Wiimote::GetClassicGroup(port_num, WiimoteEmu::ClassicGroup::Triggers), this, this);

  auto* const controls_sizer = new wxBoxSizer(wxHORIZONTAL);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(group_box_buttons, 0, wxEXPAND);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(group_box_dpad, 0, wxEXPAND);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(group_box_left_stick, 0, wxEXPAND);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(group_box_right_stick, 0, wxEXPAND);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(group_box_triggers, 0, wxEXPAND);
  controls_sizer->AddSpacer(space5);

  auto* const szr_main = new wxBoxSizer(wxVERTICAL);
  szr_main->AddSpacer(space5);
  szr_main->Add(controls_sizer, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr_main->AddSpacer(space5);
  szr_main->Add(CreateButtonSizer(wxCLOSE | wxNO_DEFAULT), 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr_main->AddSpacer(space5);

  SetSizerAndFit(szr_main);
  Center();
  UpdateGUI();
}
