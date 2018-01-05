// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/notebook.h>
#include <wx/panel.h>

#include "DolphinWX/Input/WiimoteInputConfigDiag.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "InputCommon/ControllerEmu/Setting/BooleanSetting.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

WiimoteInputConfigDialog::WiimoteInputConfigDialog(wxWindow* const parent, InputConfig& config,
                                                   const wxString& name, const int port_num)
    : InputConfigDialog(parent, config, name, port_num)
{
  const int space5 = FromDIP(5);

  auto* const device_chooser = CreateDeviceChooserGroupBox();
  auto* const reset_sizer = CreaterResetGroupBox(wxVERTICAL);
  auto* const profile_chooser = CreateProfileChooserGroupBox();

  auto* const notebook = new wxNotebook(this, wxID_ANY);

  // General and Options
  auto* const tab_general = new wxPanel(notebook);

  auto* const group_box_buttons = new ControlGroupBox(
      Wiimote::GetWiimoteGroup(port_num, WiimoteEmu::WiimoteGroup::Buttons), tab_general, this);
  auto* const group_box_dpad = new ControlGroupBox(
      Wiimote::GetWiimoteGroup(port_num, WiimoteEmu::WiimoteGroup::DPad), tab_general, this);
  auto* const group_box_rumble = new ControlGroupBox(
      Wiimote::GetWiimoteGroup(port_num, WiimoteEmu::WiimoteGroup::Rumble), tab_general, this);
  auto* const group_box_extension = new ControlGroupBox(
      Wiimote::GetWiimoteGroup(port_num, WiimoteEmu::WiimoteGroup::Extension), tab_general, this);
  auto* const group_box_options = new ControlGroupBox(
      Wiimote::GetWiimoteGroup(port_num, WiimoteEmu::WiimoteGroup::Options), tab_general, this);
  auto* const group_box_hotkeys = new ControlGroupBox(
      Wiimote::GetWiimoteGroup(port_num, WiimoteEmu::WiimoteGroup::Hotkeys), tab_general, this);

  auto* const dpad_extension_rumble_sizer = new wxBoxSizer(wxVERTICAL);
  dpad_extension_rumble_sizer->Add(group_box_dpad, 0, wxEXPAND);
  dpad_extension_rumble_sizer->AddSpacer(space5);
  dpad_extension_rumble_sizer->Add(group_box_extension, 0, wxEXPAND);
  dpad_extension_rumble_sizer->AddSpacer(space5);
  dpad_extension_rumble_sizer->Add(group_box_rumble, 0, wxEXPAND);

  auto* const options_hotkeys_sizer = new wxBoxSizer(wxVERTICAL);
  options_hotkeys_sizer->Add(group_box_options, 0, wxEXPAND);
  options_hotkeys_sizer->AddSpacer(space5);
  options_hotkeys_sizer->Add(group_box_hotkeys, 0, wxEXPAND);

  auto* const general_options_sizer = new wxBoxSizer(wxHORIZONTAL);
  general_options_sizer->AddSpacer(space5);
  general_options_sizer->Add(group_box_buttons, 0, wxEXPAND | wxTOP, space5);
  general_options_sizer->AddSpacer(space5);
  general_options_sizer->Add(dpad_extension_rumble_sizer, 0, wxEXPAND | wxTOP, space5);
  general_options_sizer->AddSpacer(space5);
  general_options_sizer->Add(options_hotkeys_sizer, 0, wxEXPAND | wxTOP, space5);
  general_options_sizer->AddSpacer(space5);

  tab_general->SetSizerAndFit(general_options_sizer);

  notebook->AddPage(tab_general, _("General and Options"));

  // Motion Controls and IR
  auto* const tab_motion_controls_ir = new wxPanel(notebook);

  auto* const group_box_shake =
      new ControlGroupBox(Wiimote::GetWiimoteGroup(port_num, WiimoteEmu::WiimoteGroup::Shake),
                          tab_motion_controls_ir, this);
  auto* const group_box_ir =
      new ControlGroupBox(Wiimote::GetWiimoteGroup(port_num, WiimoteEmu::WiimoteGroup::IR),
                          tab_motion_controls_ir, this);
  auto* const group_box_tilt =
      new ControlGroupBox(Wiimote::GetWiimoteGroup(port_num, WiimoteEmu::WiimoteGroup::Tilt),
                          tab_motion_controls_ir, this);
  auto* const group_box_swing =
      new ControlGroupBox(Wiimote::GetWiimoteGroup(port_num, WiimoteEmu::WiimoteGroup::Swing),
                          tab_motion_controls_ir, this);

  auto* const swing_shake_sizer = new wxBoxSizer(wxVERTICAL);
  swing_shake_sizer->Add(group_box_swing, 0, wxEXPAND);
  swing_shake_sizer->AddSpacer(space5);
  swing_shake_sizer->Add(group_box_shake, 0, wxEXPAND);

  auto* const motion_controls_ir_sizer = new wxBoxSizer(wxHORIZONTAL);
  motion_controls_ir_sizer->AddSpacer(space5);
  motion_controls_ir_sizer->Add(group_box_ir, 0, wxEXPAND | wxTOP, space5);
  motion_controls_ir_sizer->AddSpacer(space5);
  motion_controls_ir_sizer->Add(group_box_tilt, 0, wxEXPAND | wxTOP, space5);
  motion_controls_ir_sizer->AddSpacer(space5);
  motion_controls_ir_sizer->Add(swing_shake_sizer, 0, wxEXPAND | wxTOP, space5);
  motion_controls_ir_sizer->AddSpacer(space5);

  tab_motion_controls_ir->SetSizerAndFit(motion_controls_ir_sizer);

  // i18n: IR stands for infrared and refers to the pointer functionality of Wii Remotes
  notebook->AddPage(tab_motion_controls_ir, _("Motion Controls and IR"));

  notebook->SetSelection(0);

  auto* const device_profile_sizer = new wxBoxSizer(wxVERTICAL);
  device_profile_sizer->Add(device_chooser, 1, wxEXPAND);
  device_profile_sizer->Add(profile_chooser, 1, wxEXPAND);

  auto* const dio = new wxBoxSizer(wxHORIZONTAL);
  dio->AddSpacer(space5);
  dio->Add(device_profile_sizer, 5, wxEXPAND);
  dio->AddSpacer(space5);
  dio->Add(reset_sizer, 1, wxEXPAND);
  dio->AddSpacer(space5);

  auto* const szr_main = new wxBoxSizer(wxVERTICAL);
  szr_main->AddSpacer(space5);
  szr_main->Add(dio);
  szr_main->AddSpacer(space5);
  szr_main->Add(notebook, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
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
