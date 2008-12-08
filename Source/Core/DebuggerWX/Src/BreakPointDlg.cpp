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

#include "BreakPointDlg.h"
#include "Common.h"
#include "Debugger.h"
#include "StringUtil.h"
#include "Debugger/Debugger_BreakPoints.h"


BEGIN_EVENT_TABLE(BreakPointDlg,wxDialog)
	EVT_CLOSE(BreakPointDlg::OnClose)
	EVT_BUTTON(ID_OK, BreakPointDlg::OnOK)
	EVT_BUTTON(ID_CANCEL, BreakPointDlg::OnCancel)
END_EVENT_TABLE()


BreakPointDlg::BreakPointDlg(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
	CreateGUIControls();
}


BreakPointDlg::~BreakPointDlg()
{
} 


void BreakPointDlg::CreateGUIControls()
{
	SetIcon(wxNullIcon);
	SetSize(8,8,279,121);
	Center();
	

	wxStaticText* WxStaticText1 = new wxStaticText(this, ID_WXSTATICTEXT1, wxT("Address"), wxPoint(8,24), wxDefaultSize, 0, wxT("WxStaticText1"));
	WxStaticText1->SetFont(wxFont(8, wxSWISS, wxNORMAL,wxNORMAL, false, wxT("Tahoma")));

	m_pButtonOK = new wxButton(this, ID_OK, wxT("OK"), wxPoint(192,64), wxSize(73,25), 0, wxDefaultValidator, wxT("OK"));
	m_pButtonOK->SetFont(wxFont(8, wxSWISS, wxNORMAL,wxNORMAL, false, wxT("Tahoma")));

	m_pButtonCancel = new wxButton(this, ID_CANCEL, wxT("Cancel"), wxPoint(112,64), wxSize(73,25), 0, wxDefaultValidator, wxT("Cancel"));
	m_pButtonCancel->SetFont(wxFont(8, wxSWISS, wxNORMAL,wxNORMAL, false, wxT("Tahoma")));

	m_pEditAddress = new wxTextCtrl(this, ID_ADDRESS, wxT("80000000"), wxPoint(56,24), wxSize(197,20), 0, wxDefaultValidator, wxT("WxEdit1"));
	m_pEditAddress->SetFont(wxFont(8, wxSWISS, wxNORMAL,wxNORMAL, false, wxT("Tahoma")));

	wxStaticBox* WxStaticBox1 = new wxStaticBox(this, ID_WXSTATICBOX1, wxT("Address"), wxPoint(0,0), wxSize(265,57));
	WxStaticBox1->SetFont(wxFont(8, wxSWISS, wxNORMAL,wxNORMAL, false, wxT("Tahoma")));
}


void BreakPointDlg::OnClose(wxCloseEvent& /*event*/)
{
	Destroy();
}

void BreakPointDlg::OnOK(wxCommandEvent& /*event*/)
{
	wxString AddressString = m_pEditAddress->GetLineText(0);
	u32 Address = 0;
	if (AsciiToHex(AddressString.mb_str(), Address))
	{
		CBreakPoints::AddBreakPoint(Address);
		CBreakPoints::UpdateBreakPointView();
		Close();		
	}
}

void BreakPointDlg::OnCancel(wxCommandEvent& /*event*/)
{
	Close();
}
