// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/GCPadInputConfigDiag.h"

#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"

GCPadInputConfigDialog::GCPadInputConfigDialog(wxWindow* const parent, InputConfig& config,
                                               const wxString& name, const int port_num)
    : InputConfigDialog(parent, config, name, port_num)
{
  const int space5 = FromDIP(5);

  wxBoxSizer* device_chooser = CreateDeviceChooserGroupBox();
  wxBoxSizer* reset_sizer = CreaterResetGroupBox(wxHORIZONTAL);
  wxBoxSizer* profile_chooser = CreateProfileChooserGroupBox();

  ControlGroupBox* group_box_buttons =
      new ControlGroupBox(Pad::GetGroup(port_num, PADGP_BUTTONS), this, this);
  ControlGroupBox* group_box_main_stick =
      new ControlGroupBox(Pad::GetGroup(port_num, PADGP_MAIN_STICK), this, this);
  ControlGroupBox* group_box_c_stick =
      new ControlGroupBox(Pad::GetGroup(port_num, PADGP_C_STICK), this, this);
  ControlGroupBox* group_box_dpad =
      new ControlGroupBox(Pad::GetGroup(port_num, PADGP_DPAD), this, this);
  ControlGroupBox* group_box_triggers =
      new ControlGroupBox(Pad::GetGroup(port_num, PADGP_TRIGGERS), this, this);
  ControlGroupBox* group_box_rumble =
      new ControlGroupBox(Pad::GetGroup(port_num, PADGP_RUMBLE), this, this);
  ControlGroupBox* group_box_options =
      new ControlGroupBox(Pad::GetGroup(port_num, PADGP_OPTIONS), this, this);

  wxBoxSizer* triggers_rumble_sizer = new wxBoxSizer(wxVERTICAL);
  triggers_rumble_sizer->Add(group_box_triggers, 0, wxEXPAND);
  triggers_rumble_sizer->AddSpacer(space5);
  triggers_rumble_sizer->Add(group_box_rumble, 0, wxEXPAND);

  wxBoxSizer* dpad_options_sizer = new wxBoxSizer(wxVERTICAL);
  dpad_options_sizer->Add(group_box_dpad, 0, wxEXPAND);
  dpad_options_sizer->AddSpacer(space5);
  dpad_options_sizer->Add(group_box_options, 0, wxEXPAND);

  wxBoxSizer* controls_sizer = new wxBoxSizer(wxHORIZONTAL);
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

  wxBoxSizer* const dio = new wxBoxSizer(wxHORIZONTAL);
  dio->AddSpacer(space5);
  dio->Add(device_chooser, 1, wxEXPAND);
  dio->AddSpacer(space5);
  dio->Add(reset_sizer, 0, wxEXPAND);
  dio->AddSpacer(space5);
  dio->Add(profile_chooser, 1, wxEXPAND);
  dio->AddSpacer(space5);

  wxBoxSizer* szr_main = new wxBoxSizer(wxVERTICAL);
  szr_main->AddSpacer(space5);
  szr_main->Add(dio);
  szr_main->AddSpacer(space5);
  szr_main->Add(controls_sizer, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr_main->AddSpacer(space5);
  szr_main->Add(CreateButtonSizer(wxCLOSE | wxNO_DEFAULT), 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr_main->AddSpacer(space5);

  SetSizerAndFit(szr_main);
  Center();

  UpdateDeviceComboBox();
  UpdateProfileComboBox();

  UpdateGUI();
}
