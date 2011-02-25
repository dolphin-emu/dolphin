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
#include "Host.h"
#include "PowerPC/PowerPC.h"

BEGIN_EVENT_TABLE(MemoryCheckDlg,wxDialog)
	EVT_CLOSE(MemoryCheckDlg::OnClose)
	EVT_BUTTON(ID_OK, MemoryCheckDlg::OnOK)
	EVT_BUTTON(ID_CANCEL, MemoryCheckDlg::OnCancel)
END_EVENT_TABLE()


MemoryCheckDlg::MemoryCheckDlg(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
	CreateGUIControls();
}

MemoryCheckDlg::~MemoryCheckDlg()
{
} 

void MemoryCheckDlg::CreateGUIControls()
{
	SetIcon(wxNullIcon);
	SetSize(8,8,470,122);
	Center();
	
	m_pButtonCancel = new wxButton(this, ID_CANCEL, _("Cancel"), wxPoint(248,64), wxSize(73,25), 0, wxDefaultValidator, _("Cancel"));

	m_pButtonOK = new wxButton(this, ID_OK, wxT("OK"), wxPoint(328,64), wxSize(73,25), 0, wxDefaultValidator, wxT("OK"));

	m_pReadFlag = new wxCheckBox(this, ID_READ_FLAG, _("Read"), wxPoint(336,33), wxSize(57,15), 0, wxDefaultValidator, _("Read"));

	m_pWriteFlag = new wxCheckBox(this, ID_WRITE_FLAG, _("Write"), wxPoint(336,16), wxSize(57,17), 0, wxDefaultValidator, wxT("WxCheckBox1"));

	m_log_flag = new wxCheckBox(this, ID_LOG_FLAG, _("Log"), wxPoint(420,33), wxSize(57,13), 0, wxDefaultValidator, wxT("WxCheckBox2"));

	new wxStaticBox(this, ID_WXSTATICBOX2, _("Break On"), wxPoint(328,0), wxSize(73,57));

	new wxStaticText(this, ID_WXSTATICTEXT2, _("End"), wxPoint(168,24), wxDefaultSize, 0, wxT("WxStaticText2"));

	new wxStaticText(this, ID_WXSTATICTEXT1, _("Start"), wxPoint(8,24), wxDefaultSize, 0, wxT("WxStaticText1"));

	m_pEditStartAddress = new wxTextCtrl(this, ID_EDIT_START_ADDR, wxT(""), wxPoint(40,24), wxSize(109,20), 0, wxDefaultValidator, wxT("WxEdit1"));

	m_pEditEndAddress = new wxTextCtrl(this, ID_EDIT_END_ADDRESS, wxT(""), wxPoint(200,24), wxSize(109,20), 0, wxDefaultValidator, wxT("WxEdit2"));

	new wxStaticBox(this, ID_WXSTATICBOX1, _("Address Range"), wxPoint(0,0), wxSize(321,57));
}

void MemoryCheckDlg::OnClose(wxCloseEvent& /*event*/)
{
	Destroy();
}

void MemoryCheckDlg::OnOK(wxCommandEvent& /*event*/)
{
	wxString StartAddressString = m_pEditStartAddress->GetLineText(0);
	wxString EndAddressString = m_pEditEndAddress->GetLineText(0);
	bool OnRead = m_pReadFlag->GetValue();
	bool OnWrite = m_pWriteFlag->GetValue();
	bool OnLog = m_log_flag->GetValue();

	u32 StartAddress, EndAddress;
	if (AsciiToHex(StartAddressString.mb_str(), StartAddress) &&
		AsciiToHex(EndAddressString.mb_str(), EndAddress))
	{
		TMemCheck MemCheck;
		MemCheck.StartAddress = StartAddress;
		MemCheck.EndAddress = EndAddress;
		MemCheck.bRange = StartAddress != EndAddress;
		MemCheck.OnRead = OnRead;
		MemCheck.OnWrite = OnWrite;

		MemCheck.Log = OnLog;
		MemCheck.Break = OnRead || OnWrite;

		PowerPC::memchecks.Add(MemCheck);
		Host_UpdateBreakPointView();
		Close();
	}
}

void MemoryCheckDlg::OnCancel(wxCommandEvent& /*event*/)
{
	Close();
}
