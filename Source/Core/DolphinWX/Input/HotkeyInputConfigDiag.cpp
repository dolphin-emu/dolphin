// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>

#include "Core/HotkeyManager.h"
#include "DolphinWX/Input/HotkeyInputConfigDiag.h"

enum HotkeyGroup : int;

HotkeyInputConfigDialog::HotkeyInputConfigDialog(wxWindow* const parent, InputConfig& config,
                                                 const wxString& name, const int port_num)
    : InputConfigDialog(parent, config, name, port_num)
{
  const int space5 = FromDIP(5);

  auto* const device_chooser = CreateDeviceChooserGroupBox();
  auto* const reset_sizer = CreaterResetGroupBox(wxVERTICAL);
  auto* const profile_chooser = CreateProfileChooserGroupBox();

  auto* const notebook = new wxNotebook(this, wxID_ANY);

  // General
  auto* const tab_general = new wxPanel(notebook);

  auto* const group_box_general =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_GENERAL), tab_general, this);
  auto* const group_box_volume =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_VOLUME), tab_general, this);
  auto* const group_box_speed =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_SPEED), tab_general, this);

  auto* const szr_volume_speed = new wxBoxSizer(wxVERTICAL);
  szr_volume_speed->AddSpacer(space5);
  szr_volume_speed->Add(group_box_volume, 0, wxEXPAND);
  szr_volume_speed->AddSpacer(space5);
  szr_volume_speed->Add(group_box_speed, 0, wxEXPAND);

  auto* const szr_general = new wxBoxSizer(wxHORIZONTAL);
  szr_general->Add(group_box_general, 0, wxEXPAND | wxTOP, space5);
  szr_general->AddSpacer(space5);
  szr_general->Add(szr_volume_speed, 0, wxEXPAND);

  tab_general->SetSizerAndFit(szr_general);

  notebook->AddPage(tab_general, _("General"));

  // TAS Tools
  auto* const tab_tas = new wxPanel(notebook);

  auto* const group_box_frame_advance =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_FRAME_ADVANCE), tab_tas, this);
  auto* const group_box_movie =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_MOVIE), tab_tas, this);

  auto* const szr_tas = new wxBoxSizer(wxHORIZONTAL);
  szr_tas->AddSpacer(space5);
  szr_tas->Add(group_box_frame_advance, 0, wxEXPAND | wxTOP, space5);
  szr_tas->AddSpacer(space5);
  szr_tas->Add(group_box_movie, 0, wxEXPAND | wxTOP, space5);

  tab_tas->SetSizerAndFit(szr_tas);

  // i18n: TAS is short for tool-assisted speedrun. Read http://tasvideos.org/ for details.
  // Frame advance is an example of a typical TAS tool.
  notebook->AddPage(tab_tas, _("TAS Tools"));

  // WII and Wii Remote
  auto* const tab_wii = new wxPanel(notebook);

  auto* const group_box_wii =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_WII), tab_wii, this);

  auto* const szr_wii = new wxBoxSizer(wxHORIZONTAL);
  szr_wii->AddSpacer(space5);
  szr_wii->Add(group_box_wii, 0, wxEXPAND | wxTOP, space5);
  szr_wii->AddSpacer(space5);

  tab_wii->SetSizerAndFit(szr_wii);

  notebook->AddPage(tab_wii, _("Wii and Wii Remote"));

  // Graphics
  auto* const tab_graphics = new wxPanel(notebook);

  auto* const group_box_graphics_toggles = new ControlGroupBox(
      HotkeyManagerEmu::GetHotkeyGroup(HKGP_GRAPHICS_TOGGLES), tab_graphics, this);
  auto* const group_box_ir =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_IR), tab_graphics, this);

  auto* const szr_toggle_ir = new wxBoxSizer(wxVERTICAL);
  szr_toggle_ir->Add(group_box_graphics_toggles, 0, wxEXPAND);
  szr_toggle_ir->AddSpacer(space5);
  szr_toggle_ir->Add(group_box_ir, 0, wxEXPAND);

  auto* const group_box_freelook =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_FREELOOK), tab_graphics, this);

  auto* const szr_graphics_toggles = new wxBoxSizer(wxHORIZONTAL);
  szr_graphics_toggles->AddSpacer(space5);
  szr_graphics_toggles->Add(szr_toggle_ir, 0, wxEXPAND | wxTOP, space5);
  szr_graphics_toggles->AddSpacer(space5);
  szr_graphics_toggles->Add(group_box_freelook, 0, wxEXPAND | wxTOP, space5);
  szr_graphics_toggles->AddSpacer(space5);

  tab_graphics->SetSizerAndFit(szr_graphics_toggles);

  notebook->AddPage(tab_graphics, _("Graphics"));

  // 3D
  auto* const tab_3D = new wxPanel(notebook);

  auto* const group_box_3D_toggles =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_3D_TOGGLE), tab_3D, this);
  auto* const group_box_3D_depth =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_3D_DEPTH), tab_3D, this);

  auto* const szr_3D = new wxBoxSizer(wxHORIZONTAL);
  szr_3D->AddSpacer(space5);
  szr_3D->Add(group_box_3D_toggles, 0, wxEXPAND | wxTOP, space5);
  szr_3D->AddSpacer(space5);
  szr_3D->Add(group_box_3D_depth, 0, wxEXPAND | wxTOP, space5);
  szr_3D->AddSpacer(space5);

  tab_3D->SetSizerAndFit(szr_3D);

  // i18n: Stereoscopic 3D
  notebook->AddPage(tab_3D, _("3D"));

  // Save and Load State
  auto* const tab_save_load_state = new wxPanel(notebook);

  auto* const group_box_load_state = new ControlGroupBox(
      HotkeyManagerEmu::GetHotkeyGroup(HKGP_LOAD_STATE), tab_save_load_state, this);
  auto* const group_box_save_state = new ControlGroupBox(
      HotkeyManagerEmu::GetHotkeyGroup(HKGP_SAVE_STATE), tab_save_load_state, this);

  auto* const szr_save_load_state = new wxBoxSizer(wxHORIZONTAL);
  szr_save_load_state->AddSpacer(space5);
  szr_save_load_state->Add(group_box_load_state, 0, wxEXPAND | wxTOP, space5);
  szr_save_load_state->AddSpacer(space5);
  szr_save_load_state->Add(group_box_save_state, 0, wxEXPAND | wxTOP, space5);
  szr_save_load_state->AddSpacer(space5);

  tab_save_load_state->SetSizerAndFit(szr_save_load_state);

  notebook->AddPage(tab_save_load_state, _("Save and Load State"));

  // Other State Managament
  auto* const tab_other_state = new wxPanel(notebook);

  auto* const group_box_select_state = new ControlGroupBox(
      HotkeyManagerEmu::GetHotkeyGroup(HKGP_SELECT_STATE), tab_other_state, this);
  auto* const group_box_load_last_state = new ControlGroupBox(
      HotkeyManagerEmu::GetHotkeyGroup(HKGP_LOAD_LAST_STATE), tab_other_state, this);
  auto* const group_box_state_misc =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_STATE_MISC), tab_other_state, this);

  auto* const szr_other_state = new wxBoxSizer(wxHORIZONTAL);
  szr_other_state->AddSpacer(space5);
  szr_other_state->Add(group_box_select_state, 0, wxEXPAND | wxTOP, space5);
  szr_other_state->AddSpacer(space5);
  szr_other_state->Add(group_box_load_last_state, 0, wxEXPAND | wxTOP, space5);
  szr_other_state->AddSpacer(space5);
  szr_other_state->Add(group_box_state_misc, 0, wxEXPAND | wxTOP, space5);
  szr_other_state->AddSpacer(space5);

  tab_other_state->SetSizerAndFit(szr_other_state);

  notebook->AddPage(tab_other_state, _("Other State Management"));

  notebook->SetSelection(0);

  auto* const device_profile_sizer = new wxBoxSizer(wxVERTICAL);
  device_profile_sizer->Add(device_chooser, 1, wxEXPAND);
  device_profile_sizer->Add(profile_chooser, 1, wxEXPAND);

  auto* const group_box_options =
      new ControlGroupBox(HotkeyManagerEmu::GetOptionsGroup(), this, this);

  auto* const dio = new wxBoxSizer(wxHORIZONTAL);
  dio->AddSpacer(space5);
  dio->Add(device_profile_sizer, 3, wxEXPAND);
  dio->Add(reset_sizer, 0, wxEXPAND);
  dio->Add(group_box_options, 1, wxEXPAND);
  dio->AddSpacer(space5);

  auto* const szr_main = new wxBoxSizer(wxVERTICAL);
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
