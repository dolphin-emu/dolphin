// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "BreakpointDlg.h"
#include "StringUtil.h"
#include "PowerPC/PowerPC.h"
#include "BreakpointWindow.h"
#include "../WxUtils.h"

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
	if (AsciiToHex(WxStrToStr(AddressString).c_str(), Address))
	{
		PowerPC::breakpoints.Add(Address);
		Parent->NotifyUpdate();
		Close();
	}
	else
	{
		PanicAlert("The address %s is invalid.", WxStrToStr(AddressString).c_str());
	}

	event.Skip();
}
