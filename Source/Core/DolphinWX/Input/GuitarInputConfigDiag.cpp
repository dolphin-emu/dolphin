// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Input/GuitarInputConfigDiag.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

GuitarInputConfigDialog::GuitarInputConfigDialog(wxWindow* const parent, InputConfig& config,
                                                 const wxString& name,
                                                 wxComboBox* device_cbox_parent, const int port_num)
    : InputConfigDialog(parent, config, name, port_num)
{
  const int space5 = FromDIP(5);

  device_cbox = device_cbox_parent;

  auto* const group_box_buttons = new ControlGroupBox(
      Wiimote::GetGuitarGroup(port_num, WiimoteEmu::GuitarGroup::Buttons), this, this);
  auto* const group_left_strum = new ControlGroupBox(
      Wiimote::GetGuitarGroup(port_num, WiimoteEmu::GuitarGroup::Strum), this, this);
  auto* const group_slider_bar = new ControlGroupBox(
      Wiimote::GetGuitarGroup(port_num, WiimoteEmu::GuitarGroup::SliderBar), this, this);

  auto* const buttons_strum_slider_bar_sizer = new wxBoxSizer(wxVERTICAL);
  buttons_strum_slider_bar_sizer->Add(group_box_buttons, 0, wxEXPAND);
  buttons_strum_slider_bar_sizer->AddSpacer(space5);
  buttons_strum_slider_bar_sizer->Add(group_left_strum, 0, wxEXPAND);
  buttons_strum_slider_bar_sizer->AddSpacer(space5);
  buttons_strum_slider_bar_sizer->Add(group_slider_bar, 0, wxEXPAND);

  auto* const group_box_frets = new ControlGroupBox(
      Wiimote::GetGuitarGroup(port_num, WiimoteEmu::GuitarGroup::Frets), this, this);
  ControlGroupBox* group_box_whammy = new ControlGroupBox(
      Wiimote::GetGuitarGroup(port_num, WiimoteEmu::GuitarGroup::Whammy), this, this);

  auto* const frets_whammy_sizer = new wxBoxSizer(wxVERTICAL);
  frets_whammy_sizer->Add(group_box_frets, 0, wxEXPAND);
  frets_whammy_sizer->AddSpacer(space5);
  frets_whammy_sizer->Add(group_box_whammy, 0, wxEXPAND);

  auto* const group_box_stick = new ControlGroupBox(
      Wiimote::GetGuitarGroup(port_num, WiimoteEmu::GuitarGroup::Stick), this, this);

  auto* const controls_sizer = new wxBoxSizer(wxHORIZONTAL);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(buttons_strum_slider_bar_sizer, 0, wxEXPAND);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(frets_whammy_sizer, 0, wxEXPAND);
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
