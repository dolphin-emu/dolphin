// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "ARCodeAddEdit.h"

extern std::vector<ActionReplay::ARCode> arCodes;

BEGIN_EVENT_TABLE(CARCodeAddEdit, wxDialog)
	EVT_CLOSE(CARCodeAddEdit::OnClose)
	EVT_SPIN(ID_ENTRY_SELECT, CARCodeAddEdit::ChangeEntry)
END_EVENT_TABLE()

CARCodeAddEdit::CARCodeAddEdit(int _selection, wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	selection  = _selection;
	CreateGUIControls(selection);
}

CARCodeAddEdit::~CARCodeAddEdit()
{
}

void CARCodeAddEdit::CreateGUIControls(int _selection)
{
	ActionReplay::ARCode currentCode = arCodes.at(_selection);

	wxBoxSizer* sEditCheat = new wxBoxSizer(wxVERTICAL);
	wxStaticBoxSizer* sbEntry = new wxStaticBoxSizer(wxVERTICAL, this, _("Code"));
	wxStaticText* EditCheatNameText = new wxStaticText(this, ID_EDITCHEAT_NAME_TEXT, _("Name:"), wxDefaultPosition, wxDefaultSize);
	EditCheatName = new wxTextCtrl(this, ID_EDITCHEAT_NAME, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
	EditCheatName->SetValue(wxString::FromAscii(currentCode.name.c_str()));
	EntrySelection = new wxSpinButton(this, ID_ENTRY_SELECT, wxDefaultPosition, wxDefaultSize, wxVERTICAL);
	EntrySelection->SetRange(0, (int)arCodes.size()-1);
	EntrySelection->SetValue(_selection);
	EditCheatCode = new wxTextCtrl(this, ID_EDITCHEAT_CODE, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
	UpdateTextCtrl(currentCode);
	wxGridBagSizer* sgEntry = new wxGridBagSizer(0, 0);
	sgEntry->AddGrowableCol(1);
	sgEntry->AddGrowableRow(1);
	sgEntry->Add(EditCheatNameText, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxEXPAND|wxALL, 5);
	sgEntry->Add(EditCheatName,		wxGBPosition(0, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sgEntry->Add(EntrySelection,	wxGBPosition(0, 2), wxGBSpan(2, 1), wxEXPAND|wxALL, 5);
	sgEntry->Add(EditCheatCode,		wxGBPosition(1, 0), wxGBSpan(1, 2), wxEXPAND|wxALL, 5);
	sbEntry->Add(sgEntry, 1, wxEXPAND);
	sEditCheat->Add(sbEntry, 1, wxEXPAND|wxALL, 5);
	wxBoxSizer* sEditCheatButtons = new wxBoxSizer(wxHORIZONTAL);
	wxButton* bOK = new wxButton(this, wxID_OK, _("OK"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	wxButton* bCancel = new wxButton(this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	sEditCheatButtons->Add(0, 0, 1, wxEXPAND, 5);
	sEditCheatButtons->Add(bOK, 0, wxALL, 5);
	sEditCheatButtons->Add(bCancel, 0, wxALL, 5);
	sEditCheat->Add(sEditCheatButtons, 0, wxEXPAND, 5);
	this->SetSizer(sEditCheat);
	sEditCheat->Layout();
}

void CARCodeAddEdit::OnClose(wxCloseEvent& WXUNUSED (event))
{
	Destroy();
}

void CARCodeAddEdit::ChangeEntry(wxSpinEvent& event)
{
	ActionReplay::ARCode currentCode = arCodes.at(event.GetPosition());
	EditCheatName->SetValue(wxString::FromAscii(currentCode.name.c_str()));
	UpdateTextCtrl(currentCode);
}

void CARCodeAddEdit::UpdateTextCtrl(ActionReplay::ARCode arCode)
{
	EditCheatCode->Clear();

	for (u32 i = 0; i < arCode.ops.size(); i++)
		EditCheatCode->AppendText(wxString::Format(wxT("%08X %08X\n"), arCode.ops.at(i).cmd_addr, arCode.ops.at(i).value));
}
