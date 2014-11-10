// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>
#include <wx/chartype.h>
#include <wx/checkbox.h>
#include <wx/defs.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/translation.h>

#include "Common/BreakPoints.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/BreakpointWindow.h"
#include "DolphinWX/Debugger/MemoryCheckDlg.h"

#define TEXT_BOX(text) new wxStaticText(this, wxID_ANY, _(text))

MemoryCheckDlg::MemoryCheckDlg(CBreakPointWindow *parent)
	: wxDialog(parent, wxID_ANY, _("Memory Check"))
	, m_parent(parent)
{
	Bind(wxEVT_BUTTON, &MemoryCheckDlg::OnOK, this);

	m_pEditStartAddress = new wxTextCtrl(this, wxID_ANY, "");
	m_pEditEndAddress = new wxTextCtrl(this, wxID_ANY, "");
	m_pWriteFlag = new wxCheckBox(this, wxID_ANY, _("Write"));
	m_pWriteFlag->SetValue(true);
	m_pReadFlag = new wxCheckBox(this, wxID_ANY, _("Read"));

	m_log_flag = new wxCheckBox(this, wxID_ANY, _("Log"));
	m_log_flag->SetValue(true);
	m_break_flag = new wxCheckBox(this, wxID_ANY, _("Break"));

	wxStaticBoxSizer *sAddressRangeBox = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Address Range"));
	sAddressRangeBox->Add(TEXT_BOX("Start"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	sAddressRangeBox->Add(m_pEditStartAddress, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
	sAddressRangeBox->Add(TEXT_BOX("End"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	sAddressRangeBox->Add(m_pEditEndAddress, 1, wxALIGN_CENTER_VERTICAL);

	wxStaticBoxSizer *sActionBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Action"));
	sActionBox->Add(m_pWriteFlag);
	sActionBox->Add(m_pReadFlag);

	wxBoxSizer* sFlags = new wxStaticBoxSizer(wxVERTICAL, this, _("Flags"));
	sFlags->Add(m_log_flag);
	sFlags->Add(m_break_flag);

	wxBoxSizer *sControls = new wxBoxSizer(wxHORIZONTAL);
	sControls->Add(sAddressRangeBox, 0, wxEXPAND);
	sControls->Add(sActionBox, 0, wxEXPAND);
	sControls->Add(sFlags, 0, wxEXPAND);

	wxBoxSizer *sMainSizer = new wxBoxSizer(wxVERTICAL);
	sMainSizer->Add(sControls, 0, wxEXPAND | wxALL, 5);
	sMainSizer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	SetSizerAndFit(sMainSizer);
	SetFocus();
}

void MemoryCheckDlg::OnOK(wxCommandEvent& event)
{
	wxString StartAddressString = m_pEditStartAddress->GetLineText(0);
	wxString EndAddressString = m_pEditEndAddress->GetLineText(0);
	bool OnRead = m_pReadFlag->GetValue();
	bool OnWrite = m_pWriteFlag->GetValue();
	bool Log = m_log_flag->GetValue();
	bool Break = m_break_flag->GetValue();

	u32 StartAddress, EndAddress;
	bool EndAddressOK = EndAddressString.Len() &&
		AsciiToHex(WxStrToStr(EndAddressString), EndAddress);

	if (AsciiToHex(WxStrToStr(StartAddressString), StartAddress) &&
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

	event.Skip();
}
