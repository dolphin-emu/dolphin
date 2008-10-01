//////////////////////////////////////////////////////////////////////////////////////////
//
// Licensetype: GNU General Public License (GPL)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.
//
// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/
//
// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
//
//////////////////////////////////////////////////////////////////////////////////////////

#include "Debugger.h"
#include "PBView.h"
#include "IniFile.h"



// =======================================================================================
// Declare events
BEGIN_EVENT_TABLE(CDebugger,wxDialog)
	EVT_CLOSE(CDebugger::OnClose)
	EVT_BUTTON(ID_UPD,CDebugger::OnUpdate)
END_EVENT_TABLE()
// =======================================================================================


CDebugger::CDebugger(wxWindow *parent, wxWindowID id, const wxString &title,
				   const wxPoint &position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
	, m_GPRListView(NULL)
{
	CreateGUIControls();

	// load ini...
	IniFile file;
	file.Load("Debugger.ini");
	this->Load(file);
}

CDebugger::~CDebugger()
{
	// empty
	IniFile file;
	file.Load("Debugger.ini");

	this->Save(file);
	file.Save("Debugger.ini");
} 

void CDebugger::Save(IniFile& _IniFile) const
{
	_IniFile.Set("SoundWindow", "x", GetPosition().x);
	_IniFile.Set("SoundWindow", "y", GetPosition().y);
	_IniFile.Set("SoundWindow", "w", GetSize().GetWidth());
	_IniFile.Set("SoundWindow", "h", GetSize().GetHeight());
}


void CDebugger::Load(IniFile& _IniFile)
{
	int x,y,w,h;
	_IniFile.Get("SoundWindow", "x", &x, GetPosition().x);
	_IniFile.Get("SoundWindow", "y", &y, GetPosition().y);
	_IniFile.Get("SoundWindow", "w", &w, GetSize().GetWidth());
	_IniFile.Get("SoundWindow", "h", &h, GetSize().GetHeight());
	SetSize(x, y, w, h);
}

void CDebugger::CreateGUIControls()
{
SetTitle(wxT("Sound Debugging"));
	SetIcon(wxNullIcon);
	SetSize(8, 8, 400, 370);
	Center();

	m_GPRListView = new CPBView(this, ID_GPR, wxDefaultPosition, GetSize(),
			wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL | wxLC_SORT_ASCENDING);

		wxBoxSizer* sMain;
		wxButton* m_Upd;
		wxButton* m_SelC;
		wxButton* m_Presets;

		wxStaticBoxSizer* sLeft;
	

	// declarations
	wxCheckBox *m_Check[2];
	wxRadioButton *m_Radio[6];

	// checkboxes
	m_Check[0] = new wxCheckBox(this, IDC_CHECK0, wxT("Save to file"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Check[1] = new wxCheckBox(this, IDC_CHECK1, wxT("Show updated"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	

	m_Radio[0] = new wxRadioButton(this, IDC_RADIO0, wxT("Show base 10"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Radio[1] = new wxRadioButton(this, IDC_RADIO1, wxT("Show base 16"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);


	m_Radio[2] = new wxRadioButton(this, IDC_RADIO2, wxT("Never"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Radio[3] = new wxRadioButton(this, IDC_RADIO3, wxT("5 times/s"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Radio[4] = new wxRadioButton(this, IDC_RADIO4, wxT("10 times/s"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);


	m_Upd = new wxButton(this, ID_UPD, wxT("Update"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_SelC = new wxButton(this, ID_SELC, wxT("Select Columns"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Presets = new wxButton(this, ID_PRESETS, wxT("Presets"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	sLeft = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Current Status"));


	wxBoxSizer* sButtons;
	sButtons = new wxBoxSizer(wxVERTICAL);

	sButtons->AddStretchSpacer(1);

	sButtons->Add(m_Upd, 0, 0, 5);
	sButtons->Add(m_SelC, 0, 0, 5);
	sButtons->Add(m_Presets, 0, 0, 5);

	sButtons->AddStretchSpacer(1);

	sButtons->Add(m_Check[0], 0, 0, 5);
	sButtons->Add(m_Check[1], 0, 0, 5);

	sButtons->AddStretchSpacer(1);

	sButtons->Add(m_Radio[0], 0, 0, 5);
	sButtons->Add(m_Radio[1], 0, 0, 5);

	sButtons->AddStretchSpacer(1);

	sButtons->Add(m_Radio[2], 0, 0, 5);
	sButtons->Add(m_Radio[3], 0, 0, 5);
	sButtons->Add(m_Radio[4], 0, 0, 5);
	
	sButtons->AddStretchSpacer(1);


	sLeft->Add(m_GPRListView, 1, wxEXPAND|wxALL, 5);	


	sMain = new wxBoxSizer(wxHORIZONTAL);
	sMain->Add(sLeft, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxEXPAND, 0);

	this->SetSizer(sMain);
	sMain->SetSizeHints(this);

	NotifyUpdate();
}

void CDebugger::OnClose(wxCloseEvent& /*event*/)
{
	EndModal(0);
}

void CDebugger::OnUpdate(wxCommandEvent& /*event*/)
{
	this->NotifyUpdate();
}

void CDebugger::NotifyUpdate()
{
	if (m_GPRListView != NULL)
	{
		m_GPRListView->Update();
	}
}
