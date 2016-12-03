// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Input/GCKeyboardInputConfigDiag.h"

#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCKeyboardEmu.h"

enum class KeyboardGroup;

GCKeyboardInputConfigDialog::GCKeyboardInputConfigDialog(wxWindow* const parent,
                                                         InputConfig& config, const wxString& name,
                                                         const int port_num)
    : InputConfigDialog(parent, config, name, port_num)
{
  const int space5 = FromDIP(5);

  auto* const device_chooser = CreateDeviceChooserGroupBox();
  auto* const reset_sizer = CreaterResetGroupBox(wxHORIZONTAL);
  auto* const profile_chooser = CreateProfileChooserGroupBox();

  auto* const group_box_keys0x =
      new ControlGroupBox(Keyboard::GetGroup(port_num, KeyboardGroup::Kb0x), this, this);
  auto* const group_box_keys1x =
      new ControlGroupBox(Keyboard::GetGroup(port_num, KeyboardGroup::Kb1x), this, this);
  auto* const group_box_keys2x =
      new ControlGroupBox(Keyboard::GetGroup(port_num, KeyboardGroup::Kb2x), this, this);
  auto* const group_box_keys3x =
      new ControlGroupBox(Keyboard::GetGroup(port_num, KeyboardGroup::Kb3x), this, this);
  auto* const group_box_keys4x =
      new ControlGroupBox(Keyboard::GetGroup(port_num, KeyboardGroup::Kb4x), this, this);
  auto* const group_box_keys5x =
      new ControlGroupBox(Keyboard::GetGroup(port_num, KeyboardGroup::Kb5x), this, this);
  auto* const group_box_options =
      new ControlGroupBox(Keyboard::GetGroup(port_num, KeyboardGroup::Options), this, this);

  auto* const key5x_options_sizer = new wxBoxSizer(wxVERTICAL);
  key5x_options_sizer->Add(group_box_keys5x);
  key5x_options_sizer->AddSpacer(space5);
  key5x_options_sizer->Add(group_box_options);

  auto* const controls_sizer = new wxBoxSizer(wxHORIZONTAL);
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

  auto* const dio = new wxBoxSizer(wxHORIZONTAL);
  dio->AddSpacer(space5);
  dio->Add(device_chooser, 3, wxEXPAND);
  dio->AddSpacer(space5);
  dio->Add(reset_sizer, 1, wxEXPAND);
  dio->AddSpacer(space5);
  dio->Add(profile_chooser, 3, wxEXPAND);
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
