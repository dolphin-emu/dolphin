// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <wx/checkbox.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/window.h>

#include "Core/ActionReplay.h"
#include "Core/ConfigManager.h"
#include "DolphinWX/ISOProperties.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Cheats/CreateCodeDialog.h"

// Fired when an ActionReplay code is created.
wxDEFINE_EVENT(UPDATE_CHEAT_LIST_EVENT, wxCommandEvent);

CreateCodeDialog::CreateCodeDialog(wxWindow* const parent, const u32 address)
	: wxDialog(parent, -1, _("Create AR Code"))
	, m_code_address(address)
{
	wxStaticText* const label_name = new wxStaticText(this, -1, _("Name: "));
	m_textctrl_name = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(256,-1));

	wxStaticText* const label_code = new wxStaticText(this, -1, _("Code: "));
	m_textctrl_code = new wxTextCtrl(this, -1, wxString::Format("0x%08x", address));
	m_textctrl_code->Disable();

	wxStaticText* const label_value = new wxStaticText(this, -1, _("Value: "));
	m_textctrl_value = new wxTextCtrl(this, -1, "0");

	m_checkbox_use_hex = new wxCheckBox(this, -1, _("Use Hex"));
	m_checkbox_use_hex->SetValue(true);

	wxBoxSizer* const sizer_value_label = new wxBoxSizer(wxHORIZONTAL);
	sizer_value_label->Add(label_value, 0, wxRIGHT, 5);
	sizer_value_label->Add(m_checkbox_use_hex);

	// main sizer
	wxBoxSizer* const sizer_main = new wxBoxSizer(wxVERTICAL);
	sizer_main->Add(label_name, 0, wxALL, 5);
	sizer_main->Add(m_textctrl_name, 0, wxALL, 5);
	sizer_main->Add(label_code, 0, wxALL, 5);
	sizer_main->Add(m_textctrl_code, 0, wxALL, 5);
	sizer_main->Add(sizer_value_label, 0, wxALL, 5);
	sizer_main->Add(m_textctrl_value, 0, wxALL, 5);
	sizer_main->Add(CreateButtonSizer(wxOK | wxCANCEL | wxNO_DEFAULT), 0, wxALL, 5);

	Bind(wxEVT_BUTTON, &CreateCodeDialog::PressOK, this, wxID_OK);
	Bind(wxEVT_BUTTON, &CreateCodeDialog::PressCancel, this, wxID_CANCEL);
	Bind(wxEVT_CLOSE_WINDOW, &CreateCodeDialog::OnEvent_Close, this);

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

	//wxString full_code = textctrl_code->GetValue();
	//full_code += ' ';
	//full_code += wxString::Format("0x%08x", code_value);

	// create the new code
	ActionReplay::ARCode new_cheat;
	new_cheat.active = false;
	new_cheat.name = WxStrToStr(code_name);
	const ActionReplay::AREntry new_entry(m_code_address, code_value);
	new_cheat.ops.push_back(new_entry);

	// pretty hacky - add the code to the gameini
	{
	CISOProperties isoprops(SConfig::GetInstance().m_LastFilename, this);
	// add the code to the isoproperties arcode list
	arCodes.push_back(new_cheat);
	// save the gameini
	isoprops.SaveGameConfig();
	isoprops.ActionReplayList_Load(); // loads the new arcodes
	//ActionReplay::UpdateActiveList();
	}

	// Propagate back to the parent frame to update the cheat list.
	GetEventHandler()->AddPendingEvent(wxCommandEvent(UPDATE_CHEAT_LIST_EVENT));

	Close();
}

void CreateCodeDialog::PressCancel(wxCommandEvent& ev)
{
	Close();
}

void CreateCodeDialog::OnEvent_Close(wxCloseEvent& ev)
{
	Destroy();
}
