// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Input/GCPadInputConfigDiag.h"

#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"

GCPadInputConfigDialog::GCPadInputConfigDialog(wxWindow* const parent, InputConfig& config,
                                               const wxString& name, const int port_num)
    : InputConfigDialog(parent, config, name, port_num)
{
  const int space5 = FromDIP(5);

  auto* const device_chooser = CreateDeviceChooserGroupBox();
  auto* const reset_sizer = CreaterResetGroupBox(wxHORIZONTAL);
  auto* const profile_chooser = CreateProfileChooserGroupBox();

  auto* const group_box_buttons =
      new ControlGroupBox(Pad::GetGroup(port_num, PadGroup::Buttons), this, this);
  auto* const group_box_main_stick =
      new ControlGroupBox(Pad::GetGroup(port_num, PadGroup::MainStick), this, this);
  auto* const group_box_c_stick =
      new ControlGroupBox(Pad::GetGroup(port_num, PadGroup::CStick), this, this);
  auto* const group_box_dpad =
      new ControlGroupBox(Pad::GetGroup(port_num, PadGroup::DPad), this, this);
  auto* const group_box_triggers =
      new ControlGroupBox(Pad::GetGroup(port_num, PadGroup::Triggers), this, this);
  auto* const group_box_rumble =
      new ControlGroupBox(Pad::GetGroup(port_num, PadGroup::Rumble), this, this);
  auto* const group_box_options =
      new ControlGroupBox(Pad::GetGroup(port_num, PadGroup::Options), this, this);

  auto* const triggers_rumble_sizer = new wxBoxSizer(wxVERTICAL);
  triggers_rumble_sizer->Add(group_box_triggers, 0, wxEXPAND);
  triggers_rumble_sizer->AddSpacer(space5);
  triggers_rumble_sizer->Add(group_box_rumble, 0, wxEXPAND);

  auto* const dpad_options_sizer = new wxBoxSizer(wxVERTICAL);
  dpad_options_sizer->Add(group_box_dpad, 0, wxEXPAND);
  dpad_options_sizer->AddSpacer(space5);
  dpad_options_sizer->Add(group_box_options, 0, wxEXPAND);

  auto* const controls_sizer = new wxBoxSizer(wxHORIZONTAL);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(group_box_buttons, 0, wxEXPAND | wxTOP, space5);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(group_box_main_stick, 0, wxEXPAND | wxTOP, space5);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(group_box_c_stick, 0, wxEXPAND | wxTOP, space5);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(triggers_rumble_sizer, 0, wxEXPAND | wxTOP, space5);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(dpad_options_sizer, 0, wxEXPAND | wxTOP, space5);
  controls_sizer->AddSpacer(space5);

  auto* const dio = new wxBoxSizer(wxHORIZONTAL);
  dio->AddSpacer(space5);
  dio->Add(device_chooser, 2, wxEXPAND);
  dio->AddSpacer(space5);
  dio->Add(reset_sizer, 1, wxEXPAND);
  dio->AddSpacer(space5);
  dio->Add(profile_chooser, 2, wxEXPAND);
  dio->AddSpacer(space5);

  auto* const szr_main = new wxBoxSizer(wxVERTICAL);
  szr_main->AddSpacer(space5);
  szr_main->Add(dio);
  szr_main->AddSpacer(space5);
  szr_main->Add(controls_sizer, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr_main->AddSpacer(space5);
  auto* const b_szr = CreateTextureChooserAndButtonBar();
  szr_main->Add(b_szr, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr_main->AddSpacer(space5);

  SetSizerAndFit(szr_main);
  Center();

  UpdateDeviceComboBox();
  UpdateProfileComboBox();

  UpdateGUI();
}
