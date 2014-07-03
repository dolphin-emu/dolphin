// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>
#include <wx/chartype.h>
#include <wx/defs.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/sizer.h>
#include <wx/string.h>
#include <wx/textctrl.h>

#include "Common/BreakPoints.h"
#include "Common/Common.h"
#include "Common/StringUtil.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/BreakpointDlg.h"
#include "DolphinWX/Debugger/BreakpointWindow.h"

BEGIN_EVENT_TABLE(BreakPointDlg, wxDialog)
	EVT_BUTTON(wxID_OK, BreakPointDlg::OnOK)
END_EVENT_TABLE()

BreakPointDlg::BreakPointDlg(CBreakPointWindow *_Parent)
	: wxDialog(_Parent, wxID_ANY, _("Add Breakpoint"))
	, Parent(_Parent)
{
	m_pEditAddress = new wxTextCtrl(this, wxID_ANY, "80000000");

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
	if (AsciiToHex(WxStrToStr(AddressString), Address))
	{
		PowerPC::breakpoints.Add(Address);
		Parent->NotifyUpdate();
		Close();
	}
	else
	{
		PanicAlertT("The address %s is invalid.", WxStrToStr(AddressString).c_str());
	}

	event.Skip();
}
