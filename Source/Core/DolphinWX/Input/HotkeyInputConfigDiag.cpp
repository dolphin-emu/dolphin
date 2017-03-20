// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/checkbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>

#include "Core/ConfigManager.h"
#include "Core/HotkeyManager.h"
#include "DolphinWX/Input/HotkeyInputConfigDiag.h"

HotkeyInputConfigDialog::HotkeyInputConfigDialog(wxWindow* parent, InputConfig& config,
                                                 const wxString& name, bool using_debugger,
                                                 int port_num)
    : InputConfigDialog(parent, config, name, port_num), m_using_debugger{using_debugger}
{
  InitializeNotebook();
  SetSizerAndFit(CreateMainSizer());
  Center();

  UpdateDeviceComboBox();
  UpdateProfileComboBox();

  UpdateGUI();
}

wxSizer* HotkeyInputConfigDialog::CreateMainSizer()
{
  const int space5 = FromDIP(5);
  auto* const main_sizer = new wxBoxSizer(wxVERTICAL);

  main_sizer->AddSpacer(space5);
  main_sizer->Add(CreateDeviceRelatedSizer(), 0, wxEXPAND | wxLEFT | wxRIGHT);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(m_notebook, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(CreateButtonSizer(wxCLOSE | wxNO_DEFAULT), 0, wxEXPAND | wxLEFT | wxRIGHT,
                  space5);
  main_sizer->AddSpacer(space5);

  return main_sizer;
}

void HotkeyInputConfigDialog::OnBackgroundInputChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_BackgroundInput = event.IsChecked();
}

void HotkeyInputConfigDialog::OnIterativeInputChanged(wxCommandEvent& event)
{
  m_iterate = event.IsChecked();
}

wxSizer* HotkeyInputConfigDialog::CreateOptionsSizer()
{
  const int space3 = FromDIP(3);

  auto background_input_checkbox = new wxCheckBox(this, wxID_ANY, _("Background Input"));
  background_input_checkbox->SetValue(SConfig::GetInstance().m_BackgroundInput);
  background_input_checkbox->Bind(wxEVT_CHECKBOX,
                                  &HotkeyInputConfigDialog::OnBackgroundInputChanged, this);

  auto iterative_input_checkbox = new wxCheckBox(this, wxID_ANY, _("Iterative Input"));
  iterative_input_checkbox->SetValue(m_iterate);
  iterative_input_checkbox->Bind(wxEVT_CHECKBOX, &HotkeyInputConfigDialog::OnIterativeInputChanged,
                                 this);

  auto sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Options"));
  sizer->AddSpacer(space3);
  sizer->Add(background_input_checkbox, 0, wxLEFT | wxRIGHT, space3);
  sizer->AddSpacer(space3);
  sizer->Add(iterative_input_checkbox, 0, wxLEFT | wxRIGHT, space3);
  sizer->AddSpacer(space3);
  return sizer;
}

wxSizer* HotkeyInputConfigDialog::CreateDeviceRelatedSizer()
{
  const int space5 = FromDIP(5);
  auto* const enclosing_sizer = new wxBoxSizer(wxHORIZONTAL);

  enclosing_sizer->AddSpacer(space5);
  enclosing_sizer->Add(CreateDeviceProfileSizer(), 3, wxEXPAND);
  enclosing_sizer->Add(CreaterResetGroupBox(wxVERTICAL), 0, wxEXPAND);
  enclosing_sizer->Add(CreateOptionsSizer(), 1, wxEXPAND);
  enclosing_sizer->AddSpacer(space5);

  return enclosing_sizer;
}

wxSizer* HotkeyInputConfigDialog::CreateDeviceProfileSizer()
{
  auto* const device_chooser = CreateDeviceChooserGroupBox();
  auto* const profile_chooser = CreateProfileChooserGroupBox();

  auto* const device_profile_sizer = new wxBoxSizer(wxVERTICAL);
  device_profile_sizer->Add(device_chooser, 1, wxEXPAND);
  device_profile_sizer->Add(profile_chooser, 1, wxEXPAND);

  return device_profile_sizer;
}

void HotkeyInputConfigDialog::InitializeNotebook()
{
  m_notebook = new wxNotebook(this, wxID_ANY);
  m_notebook->AddPage(CreateGeneralPanel(), _("General"));

  // i18n: TAS is short for tool-assisted speedrun. Read http://tasvideos.org/ for details.
  // Frame advance is an example of a typical TAS tool.
  m_notebook->AddPage(CreateTASToolsPanel(), _("TAS Tools"));

  if (m_using_debugger)
  {
    m_notebook->AddPage(CreateDebuggingPanel(), _("Debugging"));
  }

  m_notebook->AddPage(CreateWiiPanel(), _("Wii and Wii Remote"));
  m_notebook->AddPage(CreateGraphicsPanel(), _("Graphics"));

  // i18n: Stereoscopic 3D
  m_notebook->AddPage(CreateStereoscopic3DPanel(), _("3D"));
  m_notebook->AddPage(CreateSaveAndLoadStatePanel(), _("Save and Load State"));
  m_notebook->AddPage(CreateOtherStateManagementPanel(), _("Other State Management"));

  m_notebook->SetSelection(0);
}

wxPanel* HotkeyInputConfigDialog::CreateGeneralPanel()
{
  const int space5 = FromDIP(5);
  auto* const general_panel = new wxPanel(m_notebook);

  auto* const general_group_box =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_GENERAL), general_panel, this);
  auto* const volume_group_box =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_VOLUME), general_panel, this);
  auto* const speed_group_box =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_SPEED), general_panel, this);

  auto* const volume_speed_sizer = new wxBoxSizer(wxVERTICAL);
  volume_speed_sizer->AddSpacer(space5);
  volume_speed_sizer->Add(volume_group_box, 0, wxEXPAND);
  volume_speed_sizer->AddSpacer(space5);
  volume_speed_sizer->Add(speed_group_box, 0, wxEXPAND);

  auto* const general_sizer = new wxBoxSizer(wxHORIZONTAL);
  general_sizer->Add(general_group_box, 0, wxEXPAND | wxTOP, space5);
  general_sizer->AddSpacer(space5);
  general_sizer->Add(volume_speed_sizer, 0, wxEXPAND);

  general_panel->SetSizerAndFit(general_sizer);
  return general_panel;
}

wxPanel* HotkeyInputConfigDialog::CreateTASToolsPanel()
{
  const int space5 = FromDIP(5);
  auto* const tas_panel = new wxPanel(m_notebook);

  auto* const frame_advance_group_box =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_FRAME_ADVANCE), tas_panel, this);
  auto* const movie_group_box =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_MOVIE), tas_panel, this);

  auto* const tas_sizer = new wxBoxSizer(wxHORIZONTAL);
  tas_sizer->AddSpacer(space5);
  tas_sizer->Add(frame_advance_group_box, 0, wxEXPAND | wxTOP, space5);
  tas_sizer->AddSpacer(space5);
  tas_sizer->Add(movie_group_box, 0, wxEXPAND | wxTOP, space5);

  tas_panel->SetSizerAndFit(tas_sizer);
  return tas_panel;
}

wxPanel* HotkeyInputConfigDialog::CreateDebuggingPanel()
{
  const int space5 = FromDIP(5);
  auto* const debugging_panel = new wxPanel(m_notebook);

  auto* const stepping_group_box =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_STEPPING), debugging_panel, this);
  auto* const pc_group_box =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_PC), debugging_panel, this);
  auto* const breakpoint_group_box =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_BREAKPOINT), debugging_panel, this);

  auto* const debugging_sizer = new wxBoxSizer(wxHORIZONTAL);
  debugging_sizer->AddSpacer(space5);
  debugging_sizer->Add(stepping_group_box, 0, wxEXPAND | wxTOP, space5);
  debugging_sizer->AddSpacer(space5);
  debugging_sizer->Add(pc_group_box, 0, wxEXPAND | wxTOP, space5);
  debugging_sizer->AddSpacer(space5);
  debugging_sizer->Add(breakpoint_group_box, 0, wxEXPAND | wxTOP, space5);
  debugging_sizer->AddSpacer(space5);

  debugging_panel->SetSizerAndFit(debugging_sizer);
  return debugging_panel;
}

wxPanel* HotkeyInputConfigDialog::CreateWiiPanel()
{
  const int space5 = FromDIP(5);
  auto* const wii_panel = new wxPanel(m_notebook);

  auto* const wii_group_box =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_WII), wii_panel, this);

  auto* const wii_sizer = new wxBoxSizer(wxHORIZONTAL);
  wii_sizer->AddSpacer(space5);
  wii_sizer->Add(wii_group_box, 0, wxEXPAND | wxTOP, space5);
  wii_sizer->AddSpacer(space5);

  wii_panel->SetSizerAndFit(wii_sizer);
  return wii_panel;
}

wxPanel* HotkeyInputConfigDialog::CreateGraphicsPanel()
{
  const int space5 = FromDIP(5);
  auto* const graphics_panel = new wxPanel(m_notebook);

  auto* const graphics_toggles_group_box = new ControlGroupBox(
      HotkeyManagerEmu::GetHotkeyGroup(HKGP_GRAPHICS_TOGGLES), graphics_panel, this);
  auto* const ir_group_box =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_IR), graphics_panel, this);

  auto* const ir_sizer = new wxBoxSizer(wxVERTICAL);
  ir_sizer->Add(graphics_toggles_group_box, 0, wxEXPAND);
  ir_sizer->AddSpacer(space5);
  ir_sizer->Add(ir_group_box, 0, wxEXPAND);

  auto* const freelook_group_box =
      new ControlGroupBox(HotkeyManagerEmu::GetHotkeyGroup(HKGP_FREELOOK), graphics_panel, this);

  auto* const szr_graphics_toggles = new wxBoxSizer(wxHORIZONTAL);
  szr_graphics_toggles->AddSpacer(space5);
  szr_graphics_toggles->Add(ir_sizer, 0, wxEXPAND | wxTOP, space5);
  szr_graphics_toggles->AddSpacer(space5);
  szr_graphics_toggles->Add(freelook_group_box, 0, wxEXPAND | wxTOP, space5);
  szr_graphics_toggles->AddSpacer(space5);

  graphics_panel->SetSizerAndFit(szr_graphics_toggles);
  return graphics_panel;
}

wxPanel* HotkeyInputConfigDialog::CreateStereoscopic3DPanel()
{
  const int space5 = FromDIP(5);
  auto* const stereoscopic_panel = new wxPanel(m_notebook);

  auto* const stereoscopic_toggles_group_box = new ControlGroupBox(
      HotkeyManagerEmu::GetHotkeyGroup(HKGP_3D_TOGGLE), stereoscopic_panel, this);
  auto* const stereoscopic_depth_group_box = new ControlGroupBox(
      HotkeyManagerEmu::GetHotkeyGroup(HKGP_3D_DEPTH), stereoscopic_panel, this);

  auto* const stereoscopic_sizer = new wxBoxSizer(wxHORIZONTAL);
  stereoscopic_sizer->AddSpacer(space5);
  stereoscopic_sizer->Add(stereoscopic_toggles_group_box, 0, wxEXPAND | wxTOP, space5);
  stereoscopic_sizer->AddSpacer(space5);
  stereoscopic_sizer->Add(stereoscopic_depth_group_box, 0, wxEXPAND | wxTOP, space5);
  stereoscopic_sizer->AddSpacer(space5);

  stereoscopic_panel->SetSizerAndFit(stereoscopic_sizer);
  return stereoscopic_panel;
}

wxPanel* HotkeyInputConfigDialog::CreateSaveAndLoadStatePanel()
{
  const int space5 = FromDIP(5);
  auto* const save_load_state_panel = new wxPanel(m_notebook);

  auto* const load_state_group_box = new ControlGroupBox(
      HotkeyManagerEmu::GetHotkeyGroup(HKGP_LOAD_STATE), save_load_state_panel, this);
  auto* const save_state_group_box = new ControlGroupBox(
      HotkeyManagerEmu::GetHotkeyGroup(HKGP_SAVE_STATE), save_load_state_panel, this);

  auto* const save_load_state_sizer = new wxBoxSizer(wxHORIZONTAL);
  save_load_state_sizer->AddSpacer(space5);
  save_load_state_sizer->Add(load_state_group_box, 0, wxEXPAND | wxTOP, space5);
  save_load_state_sizer->AddSpacer(space5);
  save_load_state_sizer->Add(save_state_group_box, 0, wxEXPAND | wxTOP, space5);
  save_load_state_sizer->AddSpacer(space5);

  save_load_state_panel->SetSizerAndFit(save_load_state_sizer);
  return save_load_state_panel;
}

wxPanel* HotkeyInputConfigDialog::CreateOtherStateManagementPanel()
{
  const int space5 = FromDIP(5);
  auto* const other_state_panel = new wxPanel(m_notebook);

  auto* const group_box_select_state = new ControlGroupBox(
      HotkeyManagerEmu::GetHotkeyGroup(HKGP_SELECT_STATE), other_state_panel, this);
  auto* const group_box_load_last_state = new ControlGroupBox(
      HotkeyManagerEmu::GetHotkeyGroup(HKGP_LOAD_LAST_STATE), other_state_panel, this);
  auto* const group_box_state_misc = new ControlGroupBox(
      HotkeyManagerEmu::GetHotkeyGroup(HKGP_STATE_MISC), other_state_panel, this);

  auto* const other_state_sizer = new wxBoxSizer(wxHORIZONTAL);
  other_state_sizer->AddSpacer(space5);
  other_state_sizer->Add(group_box_select_state, 0, wxEXPAND | wxTOP, space5);
  other_state_sizer->AddSpacer(space5);
  other_state_sizer->Add(group_box_load_last_state, 0, wxEXPAND | wxTOP, space5);
  other_state_sizer->AddSpacer(space5);
  other_state_sizer->Add(group_box_state_misc, 0, wxEXPAND | wxTOP, space5);
  other_state_sizer->AddSpacer(space5);

  other_state_panel->SetSizerAndFit(other_state_sizer);
  return other_state_panel;
}
