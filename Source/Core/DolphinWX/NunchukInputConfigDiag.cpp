// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/NunchukInputConfigDiag.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/Attachment/Nunchuk.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

NunchukInputConfigDialog::NunchukInputConfigDialog(wxWindow* const parent, InputConfig& config,
                                                   const wxString& name, const int port_num)
    : InputConfigDialog(parent, config, name, port_num)
{
  const int space5 = FromDIP(5);

  ControlGroupBox* group_box_buttons =
      new ControlGroupBox(Wiimote::GetNunchukGroup(port_num, NUNCHUKGP_BUTTONS), this, this);
  ControlGroupBox* group_box_shake =
      new ControlGroupBox(Wiimote::GetNunchukGroup(port_num, NUNCHUKGP_SHAKE), this, this);

  wxBoxSizer* buttons_shake = new wxBoxSizer(wxVERTICAL);
  buttons_shake->Add(group_box_buttons, 0, wxEXPAND);
  buttons_shake->AddSpacer(space5);
  buttons_shake->Add(group_box_shake, 0, wxEXPAND);

  ControlGroupBox* group_box_stick =
      new ControlGroupBox(Wiimote::GetNunchukGroup(port_num, NUNCHUKGP_STICK), this, this);
  ControlGroupBox* group_box_tilt =
      new ControlGroupBox(Wiimote::GetNunchukGroup(port_num, NUNCHUKGP_TILT), this, this);
  ControlGroupBox* group_box_swing =
      new ControlGroupBox(Wiimote::GetNunchukGroup(port_num, NUNCHUKGP_SWING), this, this);

  wxBoxSizer* controls_sizer = new wxBoxSizer(wxHORIZONTAL);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(buttons_shake, 0, wxEXPAND);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(group_box_stick, 0, wxEXPAND);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(group_box_tilt, 0, wxEXPAND);
  controls_sizer->AddSpacer(space5);
  controls_sizer->Add(group_box_swing, 0, wxEXPAND);
  controls_sizer->AddSpacer(space5);

  wxBoxSizer* szr_main = new wxBoxSizer(wxVERTICAL);
  szr_main->AddSpacer(space5);
  szr_main->Add(controls_sizer, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr_main->AddSpacer(space5);
  szr_main->Add(CreateButtonSizer(wxCLOSE | wxNO_DEFAULT), 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr_main->AddSpacer(space5);

  SetSizerAndFit(szr_main);
  Center();
  UpdateGUI();
}
