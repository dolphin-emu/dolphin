// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/checkbox.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "Core/ActionReplay.h"
#include "DolphinWX/Cheats/CheatsWindow.h"
#include "DolphinWX/Cheats/CreateCodeDialog.h"
#include "DolphinWX/WxUtils.h"

CreateCodeDialog::CreateCodeDialog(wxWindow* const parent, const u32 address)
    : wxDialog(parent, wxID_ANY, _("Create AR Code")), m_code_address(address)
{
  wxStaticText* const label_name = new wxStaticText(this, wxID_ANY, _("Name: "));
  m_textctrl_name = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                   wxDLG_UNIT(this, wxSize(180, -1)));

  wxStaticText* const label_code = new wxStaticText(this, wxID_ANY, _("Code: "));
  m_textctrl_code = new wxTextCtrl(this, wxID_ANY, wxString::Format("0x%08x", address));
  m_textctrl_code->Disable();

  wxStaticText* const label_value = new wxStaticText(this, wxID_ANY, _("Value: "));
  m_textctrl_value = new wxTextCtrl(this, wxID_ANY, "0");

  m_checkbox_use_hex = new wxCheckBox(this, wxID_ANY, _("Use Hex"));
  m_checkbox_use_hex->SetValue(true);

  wxBoxSizer* const sizer_value_label = new wxBoxSizer(wxHORIZONTAL);
  const int space5 = FromDIP(5);
  sizer_value_label->Add(label_value);
  sizer_value_label->Add(m_checkbox_use_hex, 0, wxLEFT, space5);

  // main sizer
  wxBoxSizer* const sizer_main = new wxBoxSizer(wxVERTICAL);
  sizer_main->AddSpacer(space5);
  sizer_main->Add(label_name, 0, wxLEFT | wxRIGHT, space5);
  sizer_main->AddSpacer(space5);
  sizer_main->Add(m_textctrl_name, 0, wxLEFT | wxRIGHT, space5);
  sizer_main->AddSpacer(space5);
  sizer_main->Add(label_code, 0, wxLEFT | wxRIGHT, space5);
  sizer_main->AddSpacer(space5);
  sizer_main->Add(m_textctrl_code, 0, wxLEFT | wxRIGHT, space5);
  sizer_main->AddSpacer(space5);
  sizer_main->Add(sizer_value_label, 0, wxLEFT | wxRIGHT, space5);
  sizer_main->AddSpacer(space5);
  sizer_main->Add(m_textctrl_value, 0, wxLEFT | wxRIGHT, space5);
  sizer_main->AddSpacer(space5);
  sizer_main->Add(CreateButtonSizer(wxOK | wxCANCEL | wxNO_DEFAULT), 0, wxEXPAND | wxLEFT | wxRIGHT,
                  space5);
  sizer_main->AddSpacer(space5);

  // NOTE: Use default wxCANCEL handling.
  Bind(wxEVT_BUTTON, &CreateCodeDialog::PressOK, this, wxID_OK);

  SetSizerAndFit(sizer_main);
  SetFocus();
}

void CreateCodeDialog::PressOK(wxCommandEvent& ev)
{
  const wxString code_name = m_textctrl_name->GetValue();
  if (code_name.empty())
  {
    WxUtils::ShowErrorDialog(_("You must enter a name."));
    return;
  }

  long code_value;
  int base = m_checkbox_use_hex->IsChecked() ? 16 : 10;
  if (!m_textctrl_value->GetValue().ToLong(&code_value, base))
  {
    WxUtils::ShowErrorDialog(_("Invalid value."));
    return;
  }

  // create the new code
  ActionReplay::ARCode new_cheat;
  new_cheat.active = false;
  new_cheat.user_defined = true;
  new_cheat.name = WxStrToStr(code_name);
  new_cheat.ops.emplace_back(ActionReplay::AREntry(m_code_address, code_value));

  wxCommandEvent add_event(DOLPHIN_EVT_ADD_NEW_ACTION_REPLAY_CODE, GetId());
  add_event.SetClientData(&new_cheat);
  GetParent()->GetEventHandler()->ProcessEvent(add_event);

  // Allow base class to process. wxDialog will set the return code and hide the window.
  ev.Skip();
}
