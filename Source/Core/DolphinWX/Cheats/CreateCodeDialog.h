// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <wx/dialog.h>
#include <wx/event.h>

#include "Common/CommonTypes.h"

class wxCheckBox;
class wxTextCtrl;
class wxWindow;

wxDECLARE_EVENT(UPDATE_CHEAT_LIST_EVENT, wxCommandEvent);

class CreateCodeDialog final : public wxDialog
{
public:
	CreateCodeDialog(wxWindow* const parent, const u32 address);

private:
	const u32 m_code_address;

	wxTextCtrl* m_textctrl_name;
	wxTextCtrl* m_textctrl_code;
	wxTextCtrl* m_textctrl_value;
	wxCheckBox* m_checkbox_use_hex;

	void PressOK(wxCommandEvent&);
	void PressCancel(wxCommandEvent&);
	void OnEvent_Close(wxCloseEvent& ev);
};
