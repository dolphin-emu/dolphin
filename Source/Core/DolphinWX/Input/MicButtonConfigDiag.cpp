// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Input/MicButtonConfigDiag.h"

#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"

MicButtonConfigDialog::MicButtonConfigDialog(wxWindow* const parent, InputConfig& config,
                                             const wxString& name, const int port_num)
    : InputConfigDialog(parent, config, name, port_num)
{
  const int space5 = FromDIP(5);
  const int space150 = FromDIP(150);

  auto* const device_chooser = CreateDeviceChooserGroupBox();

  auto* const device_chooser_szr = new wxBoxSizer(wxHORIZONTAL);
  device_chooser_szr->AddSpacer(space5);
  device_chooser_szr->Add(device_chooser, 1, wxEXPAND);
  device_chooser_szr->AddSpacer(space5);

  auto* const group_box_button =
      new ControlGroupBox(Pad::GetGroup(port_num, PadGroup::Mic), this, this);

  auto* const controls_sizer = new wxBoxSizer(wxHORIZONTAL);
  controls_sizer->Add(group_box_button, 0, wxEXPAND);
  // Avoid having the device combo box to be too small.
  controls_sizer->AddSpacer(space150);

  auto* const szr_main = new wxBoxSizer(wxVERTICAL);
  szr_main->AddSpacer(space5);
  szr_main->Add(device_chooser_szr, 0, wxEXPAND);
  szr_main->AddSpacer(space5);
  szr_main->Add(controls_sizer, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr_main->AddSpacer(space5);
  szr_main->Add(CreateButtonSizer(wxCLOSE | wxNO_DEFAULT), 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr_main->AddSpacer(space5);

  SetSizerAndFit(szr_main);
  Center();
  UpdateGUI();
}
