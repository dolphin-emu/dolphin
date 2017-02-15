// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Input/DrumsInputConfigDiag.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

DrumsInputConfigDialog::DrumsInputConfigDialog(wxWindow* const parent, InputConfig& config,
                                               const wxString& name, wxComboBox* device_cbox_parent,
                                               const int port_num)
    : InputConfigDialog(parent, config, name, port_num)
{
  const int space5 = FromDIP(5);

  device_cbox = device_cbox_parent;

  auto* const group_box_buttons = new ControlGroupBox(
      Wiimote::GetDrumsGroup(port_num, WiimoteEmu::DrumsGroup::Buttons), this, this);
  auto* const group_box_pads = new ControlGroupBox(
      Wiimote::GetDrumsGroup(port_num, WiimoteEmu::DrumsGroup::Pads), this, this);

  auto* const buttons_pads_sizer = new wxBoxSizer(wxVERTICAL);
  buttons_pads_sizer->Add(group_box_buttons);
  buttons_pads_sizer->AddSpacer(space5);
  buttons_pads_sizer->Add(group_box_pads);

  auto* const group_box_stick = new ControlGroupBox(
      Wiimote::GetDrumsGroup(port_num, WiimoteEmu::DrumsGroup::Stick), this, this);

  auto* const controls_sizer = new wxBoxSizer(wxHORIZONTAL);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(buttons_pads_sizer, 0, wxEXPAND);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(group_box_stick, 0, wxEXPAND);
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
