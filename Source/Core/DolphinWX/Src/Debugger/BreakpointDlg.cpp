// Copyright (C) 2003 Dolphin Project.

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

#include "BreakpointDlg.h"
#include "StringUtil.h"
#include "PowerPC/PowerPC.h"
#include "BreakpointWindow.h"

BEGIN_EVENT_TABLE(BreakPointDlg, wxDialog)
	EVT_BUTTON(wxID_OK, BreakPointDlg::OnOK)
END_EVENT_TABLE()

BreakPointDlg::BreakPointDlg(CBreakPointWindow *_Parent)
	: wxDialog(_Parent, wxID_ANY, wxT("BreakPoint"), wxDefaultPosition, wxDefaultSize)
	, Parent(_Parent)
{
	m_pEditAddress = new wxTextCtrl(this, wxID_ANY, wxT("80000000"));

	wxBoxSizer *sMainSizer = new wxBoxSizer(wxVERTICAL);
	sMainSizer->Add(m_pEditAddress, 0, wxEXPAND | wxALL, 5);
	sMainSizer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxALL, 5);

	SetSizerAndFit(sMainSizer);
	SetFocus();
}

void BreakPointDlg::OnOK(wxCommandEvent& event)
{
	wxString AddressString = m_pEditAddress->GetLineText(0);
	u32 Address = 0;
	if (AsciiToHex(AddressString.mb_str(), Address))
	{
		PowerPC::breakpoints.Add(Address);
		Parent->NotifyUpdate();
		Close();		
	}
	else
		PanicAlert("The address %s is invalid.", (const char *)AddressString.ToUTF8());

	event.Skip();
}
