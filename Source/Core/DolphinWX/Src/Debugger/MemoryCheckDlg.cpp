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

#include "MemoryCheckDlg.h"
#include "Common.h"
#include "StringUtil.h"
#include "PowerPC/PowerPC.h"
#include "BreakpointWindow.h"

#define TEXT_BOX(text) new wxStaticText(this, wxID_ANY, wxT(text), wxDefaultPosition, wxDefaultSize)

BEGIN_EVENT_TABLE(MemoryCheckDlg,wxDialog)
	EVT_CLOSE(MemoryCheckDlg::OnClose)
	EVT_BUTTON(wxID_OK, MemoryCheckDlg::OnOK)
	EVT_BUTTON(wxID_CANCEL, MemoryCheckDlg::OnCancel)
END_EVENT_TABLE()

MemoryCheckDlg::MemoryCheckDlg(CBreakPointWindow *parent)
	: wxDialog(parent, wxID_ANY, _("Memory Check"), wxDefaultPosition, wxDefaultSize)
	, m_parent(parent)
{
	m_pEditStartAddress = new wxTextCtrl(this, wxID_ANY, wxT(""));
	m_pEditEndAddress = new wxTextCtrl(this, wxID_ANY, wxT(""));
	m_pWriteFlag = new wxCheckBox(this, wxID_ANY, _("Write"));
	m_pWriteFlag->SetValue(true);
	m_pReadFlag = new wxCheckBox(this, wxID_ANY, _("Read"));

	m_log_flag = new wxCheckBox(this, wxID_ANY, _("Log"));
	m_log_flag->SetValue(true);
	m_break_flag = new wxCheckBox(this, wxID_ANY, _("Break"));

	wxStaticBoxSizer *sAddressRangeBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Address Range"));
	sAddressRangeBox->Add(TEXT_BOX("Start"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	sAddressRangeBox->Add(m_pEditStartAddress, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
	sAddressRangeBox->Add(TEXT_BOX("End"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	sAddressRangeBox->Add(m_pEditEndAddress, 1, wxALIGN_CENTER_VERTICAL);

	wxStaticBoxSizer *sActionBox = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Action"));
	sActionBox->Add(m_pWriteFlag);
	sActionBox->Add(m_pReadFlag);

	wxBoxSizer* sFlags = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Flags"));
	sFlags->Add(m_log_flag);
	sFlags->Add(m_break_flag);

	wxBoxSizer* sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->AddStretchSpacer();
	sButtons->Add(new wxButton(this, wxID_CANCEL, _("Cancel")));
	sButtons->Add(new wxButton(this, wxID_OK, wxT("OK")));

	wxBoxSizer *sControls = new wxBoxSizer(wxHORIZONTAL);
	sControls->Add(sAddressRangeBox, 0, wxEXPAND);
	sControls->Add(sActionBox, 0, wxEXPAND);
	sControls->Add(sFlags, 0, wxEXPAND);

	wxBoxSizer *sMainSizer = new wxBoxSizer(wxVERTICAL);
	sMainSizer->Add(sControls, 0, wxEXPAND | wxALL, 5);
	sMainSizer->Add(sButtons, 0, wxEXPAND | wxALL, 5);

	SetSizer(sMainSizer);
	Layout();
	Fit();
}

void MemoryCheckDlg::OnClose(wxCloseEvent& WXUNUSED(event))
{
	EndModal(wxID_CLOSE);
	Destroy();
}

void MemoryCheckDlg::OnOK(wxCommandEvent& WXUNUSED(event))
{
	wxString StartAddressString = m_pEditStartAddress->GetLineText(0);
	wxString EndAddressString = m_pEditEndAddress->GetLineText(0);
	bool OnRead = m_pReadFlag->GetValue();
	bool OnWrite = m_pWriteFlag->GetValue();
	bool Log = m_log_flag->GetValue();
	bool Break = m_break_flag->GetValue();;

	u32 StartAddress, EndAddress;
	bool EndAddressOK = EndAddressString.Len() &&
		AsciiToHex(EndAddressString.mb_str(), EndAddress);

	if (AsciiToHex(StartAddressString.mb_str(), StartAddress) &&
		(OnRead || OnWrite) && (Log || Break))
	{
		TMemCheck MemCheck;

		if (!EndAddressOK)
			EndAddress = StartAddress;

		MemCheck.StartAddress = StartAddress;
		MemCheck.EndAddress = EndAddress;
		MemCheck.bRange = StartAddress != EndAddress;
		MemCheck.OnRead = OnRead;
		MemCheck.OnWrite = OnWrite;
		MemCheck.Log = Log;
		MemCheck.Break = Break;

		PowerPC::memchecks.Add(MemCheck);
		m_parent->NotifyUpdate();
		Close();
	}
}

void MemoryCheckDlg::OnCancel(wxCommandEvent& WXUNUSED(event))
{
	Close();
}
