// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/GCKeyboardInputConfigDiag.h"

#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCKeyboardEmu.h"

GCKeyboardInputConfigDialog::GCKeyboardInputConfigDialog(wxWindow* const parent,
                                                         InputConfig& config, const wxString& name,
                                                         const int port_num)
    : InputConfigDialog(parent, config, name, port_num)
{
  const int space5 = FromDIP(5);

  wxBoxSizer* device_chooser = CreateDeviceChooserGroupBox();
  wxBoxSizer* reset_sizer = CreaterResetGroupBox(wxHORIZONTAL);
  wxBoxSizer* profile_chooser = CreateProfileChooserGroupBox();

  ControlGroupBox* group_box_keys0x =
      new ControlGroupBox(Keyboard::GetGroup(port_num, KBGP_0X), this, this);
  ControlGroupBox* group_box_keys1x =
      new ControlGroupBox(Keyboard::GetGroup(port_num, KBGP_1X), this, this);
  ControlGroupBox* group_box_keys2x =
      new ControlGroupBox(Keyboard::GetGroup(port_num, KBGP_2X), this, this);
  ControlGroupBox* group_box_keys3x =
      new ControlGroupBox(Keyboard::GetGroup(port_num, KBGP_3X), this, this);
  ControlGroupBox* group_box_keys4x =
      new ControlGroupBox(Keyboard::GetGroup(port_num, KBGP_4X), this, this);
  ControlGroupBox* group_box_keys5x =
      new ControlGroupBox(Keyboard::GetGroup(port_num, KBGP_5X), this, this);
  ControlGroupBox* group_box_options =
      new ControlGroupBox(Keyboard::GetGroup(port_num, KBGP_OPTIONS), this, this);

  wxBoxSizer* key5x_options_sizer = new wxBoxSizer(wxVERTICAL);
  key5x_options_sizer->Add(group_box_keys5x);
  key5x_options_sizer->AddSpacer(space5);
  key5x_options_sizer->Add(group_box_options);

  wxBoxSizer* controls_sizer = new wxBoxSizer(wxHORIZONTAL);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(group_box_keys0x, 0, wxEXPAND | wxTOP, space5);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(group_box_keys1x, 0, wxEXPAND | wxTOP, space5);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(group_box_keys2x, 0, wxEXPAND | wxTOP, space5);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(group_box_keys3x, 0, wxEXPAND | wxTOP, space5);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(group_box_keys4x, 0, wxEXPAND | wxTOP, space5);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(key5x_options_sizer, 0, wxEXPAND | wxTOP, space5);
  controls_sizer->AddSpacer(space5);

  wxBoxSizer* const dio = new wxBoxSizer(wxHORIZONTAL);
  dio->AddSpacer(space5);
  dio->Add(device_chooser, 1, wxEXPAND);
  dio->AddSpacer(space5);
  dio->Add(reset_sizer, 1, wxEXPAND);
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
