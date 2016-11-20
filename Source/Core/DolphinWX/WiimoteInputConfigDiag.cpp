// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/notebook.h>
#include <wx/panel.h>

#include "DolphinWX/WiimoteInputConfigDiag.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

WiimoteInputConfigDialog::WiimoteInputConfigDialog(wxWindow* const parent, InputConfig& config,
                                                   const wxString& name, const int port_num)
    : InputConfigDialog(parent, config, name, port_num)
{
  const int space5 = FromDIP(5);

  wxBoxSizer* device_chooser = CreateDeviceChooserGroupBox();
  wxBoxSizer* reset_sizer = CreaterResetGroupBox(wxVERTICAL);
  wxBoxSizer* profile_chooser = CreateProfileChooserGroupBox();

  wxNotebook* notebook = new wxNotebook(this, wxID_ANY);

  // General and Options
  wxPanel* tab_general = new wxPanel(notebook);

  ControlGroupBox* group_box_buttons =
      new ControlGroupBox(Wiimote::GetWiimoteGroup(port_num, WIIMOTEGP_BUTTONS), tab_general, this);
  ControlGroupBox* group_box_dpad =
      new ControlGroupBox(Wiimote::GetWiimoteGroup(port_num, WIIMOTEGP_DPAD), tab_general, this);
  ControlGroupBox* group_box_rumble =
      new ControlGroupBox(Wiimote::GetWiimoteGroup(port_num, WIIMOTEGP_RUMBLE), tab_general, this);
  ControlGroupBox* group_box_extension = new ControlGroupBox(
      Wiimote::GetWiimoteGroup(port_num, WIIMOTEGP_EXTENSION), tab_general, this);
  ControlGroupBox* group_box_options =
      new ControlGroupBox(Wiimote::GetWiimoteGroup(port_num, WIIMOTEGP_OPTIONS), tab_general, this);
  ControlGroupBox* group_box_hotkeys =
      new ControlGroupBox(Wiimote::GetWiimoteGroup(port_num, WIIMOTEGP_HOTKEYS), tab_general, this);

  wxBoxSizer* dpad_extension_rumble_sizer = new wxBoxSizer(wxVERTICAL);
  dpad_extension_rumble_sizer->Add(group_box_dpad, 0, wxEXPAND);
  dpad_extension_rumble_sizer->AddSpacer(space5);
  dpad_extension_rumble_sizer->Add(group_box_extension, 0, wxEXPAND);
  dpad_extension_rumble_sizer->AddSpacer(space5);
  dpad_extension_rumble_sizer->Add(group_box_rumble, 0, wxEXPAND);

  wxBoxSizer* options_hotkeys_sizer = new wxBoxSizer(wxVERTICAL);
  options_hotkeys_sizer->Add(group_box_options, 0, wxEXPAND);
  options_hotkeys_sizer->AddSpacer(space5);
  options_hotkeys_sizer->Add(group_box_hotkeys, 0, wxEXPAND);

  wxBoxSizer* general_options_sizer = new wxBoxSizer(wxHORIZONTAL);
  general_options_sizer->AddSpacer(space5);
  general_options_sizer->Add(group_box_buttons, 0, wxEXPAND | wxTOP, space5);
  general_options_sizer->AddSpacer(space5);
  general_options_sizer->Add(dpad_extension_rumble_sizer, 0, wxEXPAND | wxTOP, space5);
  general_options_sizer->AddSpacer(space5);
  general_options_sizer->Add(options_hotkeys_sizer, 0, wxEXPAND | wxTOP, space5);
  general_options_sizer->AddSpacer(space5);

  tab_general->SetSizerAndFit(general_options_sizer);

  notebook->AddPage(tab_general, "General and Options");

  // Motion Controls and IR
  wxPanel* tab_motion_controls_ir = new wxPanel(notebook);

  ControlGroupBox* group_box_shake = new ControlGroupBox(
      Wiimote::GetWiimoteGroup(port_num, WIIMOTEGP_SHAKE), tab_motion_controls_ir, this);
  ControlGroupBox* group_box_ir = new ControlGroupBox(
      Wiimote::GetWiimoteGroup(port_num, WIIMOTEGP_IR), tab_motion_controls_ir, this);
  ControlGroupBox* group_box_tilt = new ControlGroupBox(
      Wiimote::GetWiimoteGroup(port_num, WIIMOTEGP_TILT), tab_motion_controls_ir, this);
  ControlGroupBox* group_box_swing = new ControlGroupBox(
      Wiimote::GetWiimoteGroup(port_num, WIIMOTEGP_SWING), tab_motion_controls_ir, this);

  wxBoxSizer* swing_shake_sizer = new wxBoxSizer(wxVERTICAL);
  swing_shake_sizer->Add(group_box_swing, 0, wxEXPAND);
  swing_shake_sizer->AddSpacer(space5);
  swing_shake_sizer->Add(group_box_shake, 0, wxEXPAND);

  wxBoxSizer* motion_controls_ir_sizer = new wxBoxSizer(wxHORIZONTAL);
  motion_controls_ir_sizer->AddSpacer(space5);
  motion_controls_ir_sizer->Add(group_box_ir, 0, wxEXPAND | wxTOP, space5);
  motion_controls_ir_sizer->AddSpacer(space5);
  motion_controls_ir_sizer->Add(group_box_tilt, 0, wxEXPAND | wxTOP, space5);
  motion_controls_ir_sizer->AddSpacer(space5);
  motion_controls_ir_sizer->Add(swing_shake_sizer, 0, wxEXPAND | wxTOP, space5);
  motion_controls_ir_sizer->AddSpacer(space5);

  tab_motion_controls_ir->SetSizerAndFit(motion_controls_ir_sizer);

  notebook->AddPage(tab_motion_controls_ir, "Motion Controls and IR");

  notebook->SetSelection(0);

  wxBoxSizer* device_profile_sizer = new wxBoxSizer(wxVERTICAL);

  device_profile_sizer->Add(device_chooser, 1, wxEXPAND);
  device_profile_sizer->Add(profile_chooser, 1, wxEXPAND);

  wxBoxSizer* const dio = new wxBoxSizer(wxHORIZONTAL);
  dio->AddSpacer(space5);
  dio->Add(device_profile_sizer, 1, wxEXPAND);
  dio->AddSpacer(space5);
  dio->Add(reset_sizer, 1, wxEXPAND);
  dio->AddSpacer(space5);

  wxBoxSizer* szr_main = new wxBoxSizer(wxVERTICAL);
  szr_main->AddSpacer(space5);
  szr_main->Add(dio);
  szr_main->AddSpacer(space5);
  szr_main->Add(notebook, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr_main->AddSpacer(space5);
  szr_main->Add(CreateButtonSizer(wxCLOSE | wxNO_DEFAULT), 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr_main->AddSpacer(space5);

  SetSizerAndFit(szr_main);
  Center();

  UpdateDeviceComboBox();
  UpdateProfileComboBox();

  UpdateGUI();
}
