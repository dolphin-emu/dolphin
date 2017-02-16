// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Input/TurntableInputConfigDiag.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

TurntableInputConfigDialog::TurntableInputConfigDialog(wxWindow* const parent, InputConfig& config,
                                                       const wxString& name,
                                                       wxComboBox* device_cbox_parent,
                                                       const int port_num)
    : InputConfigDialog(parent, config, name, port_num)
{
  const int space5 = FromDIP(5);

  device_cbox = device_cbox_parent;

  auto* const group_box_stick = new ControlGroupBox(
      Wiimote::GetTurntableGroup(port_num, WiimoteEmu::TurntableGroup::Stick), this, this);
  auto* const group_box_effect_dial = new ControlGroupBox(
      Wiimote::GetTurntableGroup(port_num, WiimoteEmu::TurntableGroup::EffectDial), this, this);

  auto* const stick_effect_sizer = new wxBoxSizer(wxVERTICAL);
  stick_effect_sizer->Add(group_box_stick, 0, wxEXPAND);
  stick_effect_sizer->AddSpacer(space5);
  stick_effect_sizer->Add(group_box_effect_dial, 0, wxEXPAND);

  auto* const group_box_left_table = new ControlGroupBox(
      Wiimote::GetTurntableGroup(port_num, WiimoteEmu::TurntableGroup::LeftTable), this, this);
  auto* const group_box_right_table = new ControlGroupBox(
      Wiimote::GetTurntableGroup(port_num, WiimoteEmu::TurntableGroup::RightTable), this, this);
  auto* const group_box_crossfade = new ControlGroupBox(
      Wiimote::GetTurntableGroup(port_num, WiimoteEmu::TurntableGroup::Crossfade), this, this);

  auto* const tables_crossfade_sizer = new wxBoxSizer(wxVERTICAL);
  tables_crossfade_sizer->Add(group_box_right_table, 0, wxEXPAND);
  tables_crossfade_sizer->AddSpacer(space5);
  tables_crossfade_sizer->Add(group_box_left_table, 0, wxEXPAND);
  tables_crossfade_sizer->AddSpacer(space5);
  tables_crossfade_sizer->Add(group_box_crossfade, 0, wxEXPAND);

  auto* const group_box_buttons = new ControlGroupBox(
      Wiimote::GetTurntableGroup(port_num, WiimoteEmu::TurntableGroup::Buttons), this, this);

  auto* const controls_sizer = new wxBoxSizer(wxHORIZONTAL);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(group_box_buttons, 0, wxEXPAND);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(stick_effect_sizer, 0, wxEXPAND);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(tables_crossfade_sizer, 0, wxEXPAND);
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
