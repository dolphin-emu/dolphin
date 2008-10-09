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

#include "Globals.h"
#include "Debugger.h"
#include "PBView.h"
#include "IniFile.h"
#include "../Logging/Console.h" // open and close console

// externals
extern int gSaveFile; // make this an int to allow multiple save file options
extern int gUpdFreq;
extern int gPreset;
int A, B;


// =======================================================================================
// Declare events
BEGIN_EVENT_TABLE(CDebugger,wxDialog)
	EVT_SHOW(CDebugger::OnShow)
	EVT_CLOSE(CDebugger::OnClose)
	EVT_BUTTON(ID_UPD,CDebugger::OnUpdate)
	EVT_CHECKBOX(IDC_CHECK0,CDebugger::SaveFile)
	EVT_CHECKBOX(IDC_CHECK2,CDebugger::ShowHideConsole)
	EVT_RADIOBOX(IDC_RADIO1,CDebugger::ChangeFrequency)
	EVT_RADIOBOX(IDC_RADIO2,CDebugger::ChangePreset)

	EVT_BUTTON(ID_AP,CDebugger::Ap)
	EVT_BUTTON(ID_AM,CDebugger::Am)
	EVT_BUTTON(ID_BP,CDebugger::Bp)
	EVT_BUTTON(ID_BM,CDebugger::Bm)
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
	// TODO1: make this work when we close the entire program to, currently on total close we get
	// weird values, perhaps because of some conflict with the rendering window
	// TODO2: get the screen resolution and make limits from that
	if(GetPosition().x < 1000 && GetPosition().y < 1000
		&& GetSize().GetWidth() < 1000 && GetSize().GetHeight() < 1000
		)
	{
		_IniFile.Set("VideoWindow", "x", GetPosition().x);
		_IniFile.Set("VideoWindow", "y", GetPosition().y);
		_IniFile.Set("VideoWindow", "w", GetSize().GetWidth());
		_IniFile.Set("VideoWindow", "h", GetSize().GetHeight());
	}		
	_IniFile.Set("VideoWindow", "Console", m_Check[2]->IsChecked()); // save settings
	_IniFile.Set("VideoWindow", "UpdateFrequency", m_RadioBox[1]->GetSelection());
}


void CDebugger::Load(IniFile& _IniFile)
{
	int x,y,w,h;
	_IniFile.Get("VideoWindow", "x", &x, GetPosition().x);
	_IniFile.Get("VideoWindow", "y", &y, GetPosition().y);
	_IniFile.Get("VideoWindow", "w", &w, GetSize().GetWidth());
	_IniFile.Get("VideoWindow", "h", &h, GetSize().GetHeight());
	SetSize(x, y, w, h);

	// saved settings
	bool Console;
	_IniFile.Get("VideoWindow", "Console", &Console, m_Check[2]->IsChecked());
	m_Check[2]->SetValue(Console);
	DoShowHideConsole();

	int UpdateFrequency;
	_IniFile.Get("VideoWindow", "UpdateFrequency", &UpdateFrequency, m_RadioBox[1]->GetSelection());
	m_RadioBox[1]->SetSelection(UpdateFrequency);
	DoChangeFrequency();
}

void CDebugger::CreateGUIControls()
{
	SetTitle(wxT("OpenGL Debugging"));

	// basic settings
	SetIcon(wxNullIcon);
	SetSize(8, 8, 200, 100); // these will become the minimin sizes allowed by resizing
	Center();

	// the big window
	m_GPRListView = new CPBView(this, ID_GPR, wxDefaultPosition, GetSize(),
			wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL | wxLC_SORT_ASCENDING);

	// declarations
	wxBoxSizer* sMain;
	wxButton* m_Upd;
	wxButton* m_Ap; wxButton* m_Am;
	wxButton* m_Bp; wxButton* m_Bm;

	wxStaticBoxSizer* sLeft;


	// buttons -----------------------------------------------------
	m_Upd = new wxButton(this, ID_UPD, wxT("Update"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Ap = new wxButton(this, ID_AP, wxT("A +"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	//m_SelC->Enable(false);
	m_Am = new wxButton(this, ID_AM, wxT("A -"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	//m_Presets->Enable(false);
	m_Bp = new wxButton(this, ID_BP, wxT("B +"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Bm = new wxButton(this, ID_BM, wxT("B -"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);


	// checkboxes and labels -----------------------------------------------------
	m_Label[0] = new wxStaticBox(this, IDG_LABEL1, wxT("Options"),
		wxDefaultPosition, wxDefaultSize, 0);
	wxStaticBoxSizer * m_checkSizer = new wxStaticBoxSizer (m_Label[0], wxVERTICAL);

	// checkboxes
	m_Check[0] = new wxCheckBox(this, IDC_CHECK0, wxT("Save to file"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Check[1] = new wxCheckBox(this, IDC_CHECK1, wxT("A +"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
		m_Check[1]->Enable(false);
	m_Check[7] = new wxCheckBox(this, IDC_CHECK7, wxT("A -"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
		m_Check[7]->Enable(false);
	m_Check[2] = new wxCheckBox(this, IDC_CHECK2, wxT("Show console"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	
    m_checkSizer->Add(m_Check[0], 0, 0, 5);
    m_checkSizer->Add(m_Check[1], 0, 0, 5);
	m_checkSizer->Add(m_Check[7], 0, 0, 5);
	m_checkSizer->Add(m_Check[2], 0, 0, 5);	
	// ------------------------

	// settings checkboxes -----------------------------------------------------
	m_Label[1] = new wxStaticBox(this, IDG_LABEL2, wxT("Settings"),
		wxDefaultPosition, wxDefaultSize, 0);
	wxStaticBoxSizer * m_checkSizer2 = new wxStaticBoxSizer (m_Label[1], wxVERTICAL);

	// checkboxes
	m_Check[3] = new wxCheckBox(this, IDC_CHECK3, wxT("Setting"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
		m_Check[3]->SetValue(false);
		m_Check[3]->Enable(false);
	
	m_checkSizer2->Add(m_Check[3], 0, 0, 5);
	// ------------------------


	// radio boxes -----------------------------------------------------
	int m_radioBoxNChoices[2];

	wxString m_radioBoxChoices0[] = { wxT("Show base 10"), wxT("Show base 16") };
	m_radioBoxNChoices[0] = sizeof( m_radioBoxChoices0 ) / sizeof( wxString );
	m_RadioBox[0] = new wxRadioBox( this, IDC_RADIO0, wxT("Show base"),
		wxDefaultPosition, wxDefaultSize, m_radioBoxNChoices[0], m_radioBoxChoices0, 1, wxRA_SPECIFY_COLS);
	m_RadioBox[0]->Enable(false);

	wxString m_radioBoxChoices1[] = { wxT("Never"), wxT("5 times/s"), wxT("15 times/s"), wxT("30 times/s") };
	m_radioBoxNChoices[1] = sizeof( m_radioBoxChoices1 ) / sizeof( wxString );
	m_RadioBox[1] = new wxRadioBox( this, IDC_RADIO1, wxT("Update freq."),
		wxDefaultPosition, wxDefaultSize, m_radioBoxNChoices[1], m_radioBoxChoices1, 1, wxRA_SPECIFY_COLS);

	wxString m_radioBoxChoices2[] = { wxT("Win stretch") };
	m_radioBoxNChoices[2] = sizeof( m_radioBoxChoices2 ) / sizeof( wxString );
	m_RadioBox[2] = new wxRadioBox( this, IDC_RADIO2, wxT("Presets"),
		wxDefaultPosition, wxDefaultSize, m_radioBoxNChoices[2], m_radioBoxChoices2, 1, wxRA_SPECIFY_COLS);
	// ------------------------


	// right buttons
	wxBoxSizer* sButtons2;
	sButtons2 = new wxBoxSizer(wxVERTICAL);

	sButtons2->AddStretchSpacer(1);
	sButtons2->Add(m_RadioBox[2], 0, 0, 5);
	sButtons2->AddStretchSpacer(1);
	sButtons2->Add(m_checkSizer2, 0, 2, 5);
	sButtons2->AddStretchSpacer(1);

	// left buttons
	wxBoxSizer* sButtons;
	sButtons = new wxBoxSizer(wxVERTICAL);

	sButtons->AddStretchSpacer(1);

	sButtons->Add(m_Upd, 0, 0, 5);
	sButtons->Add(m_Ap, 0, 0, 5);
	sButtons->Add(m_Am, 0, 0, 5);
	sButtons->Add(m_Bp, 0, 0, 5);
	sButtons->Add(m_Bm, 0, 0, 5);

	sButtons->AddStretchSpacer(1);

	sButtons->Add(m_checkSizer, 0, 2, 5);

	sButtons->AddStretchSpacer(1);

	sButtons->Add(m_RadioBox[0], 0, 0, 5);

	sButtons->AddStretchSpacer(1);

	sButtons->Add(m_RadioBox[1], 0, 0, 5);

	sButtons->AddStretchSpacer(1);

	// blocks view
	sLeft = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Current Status"));
	sLeft->Add(m_GPRListView, 1, wxEXPAND|wxALL, 5);	


	// add all stuff to the main container
	sMain = new wxBoxSizer(wxHORIZONTAL);
	sMain->Add(sLeft, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxEXPAND, 0);
	sMain->Add(sButtons2, 0, wxEXPAND, 0);

	this->SetSizer(sMain);
	sMain->SetSizeHints(this);

	//NotifyUpdate();
	Freeze(); // unfreeze this if you want to use it
}

void CDebugger::OnShow(wxShowEvent& /*event*/)
{	
	// bring the console back to
	if(m_Check[2]->IsChecked())
	{
		OpenConsole();
		MoveWindow(GetConsoleHwnd(), 0,400, 1280,500, true); // move window, TODO: make this
		// adjustable from the debugging window
	}
}

void CDebugger::OnClose(wxCloseEvent& /*event*/)
{
	// save the window position when we hide the window to
	IniFile file;
	file.Load("Debugger.ini");
	this->Save(file);
	file.Save("Debugger.ini");

	EndModal(0); // it seems like this works for Show() to, not just ShowModal();

	// The console goes with the wx window
	CloseConsole();
}

void CDebugger::OnUpdate(wxCommandEvent& /*event*/)
{
	this->NotifyUpdate();
}


// =======================================================================================
// Change preset
// --------------
void CDebugger::ChangePreset(wxCommandEvent& event)
{
	DoChangePreset();
}

void CDebugger::DoChangePreset()
{
	if(m_RadioBox[2]->GetSelection() == 0)
	{
		gPreset = 0;
	}
	else if(m_RadioBox[2]->GetSelection() == 1)
	{
		gPreset = 1;
	}
	else if(m_RadioBox[2]->GetSelection() == 2)
	{
		gPreset = 2;
	}
	else if(m_RadioBox[2]->GetSelection() == 3)
	{
		gPreset = 3;
	}
}
// ==============


// =======================================================================================
// Control variables
// --------------
void CDebugger::Ap(wxCommandEvent& event)
{
	A += 50;
	//MessageBox(0, "", "", 0);
	__Log("%i", A);
}
void CDebugger::Am(wxCommandEvent& event)
{
	A -= 50;
}
void CDebugger::Bp(wxCommandEvent& event)
{
	B += 50;
}
void CDebugger::Bm(wxCommandEvent& event)
{
	B -= 50;
}
// ==============


// =======================================================================================
// Change update frequency
// --------------
void CDebugger::ChangeFrequency(wxCommandEvent& event)
{
	DoChangeFrequency();
}

void CDebugger::DoChangeFrequency()
{
	if(m_RadioBox[1]->GetSelection() == 0)
	{
		gUpdFreq = 0;
	}
	else if(m_RadioBox[1]->GetSelection() == 1)
	{
		gUpdFreq = 5;
	}
	else if(m_RadioBox[1]->GetSelection() == 2)
	{
		gUpdFreq = 15;
	}
		else
	{
		gUpdFreq = 30;
	}
}
// ==============



// =======================================================================================
// Save to file
// --------------
void CDebugger::SaveFile(wxCommandEvent& event)
{	
	if(m_Check[0]->IsChecked())
	{
		gSaveFile = 1;

	}
	else
	{
		gSaveFile = 0;
	}
}
// ==============


// =======================================================================================
// Show or hide console window
// --------------
void CDebugger::ShowHideConsole(wxCommandEvent& event)
{
	DoShowHideConsole();
}

void CDebugger::DoShowHideConsole()
{
	
	if(m_Check[2]->IsChecked())
	{
		OpenConsole();
		MoveWindow(GetConsoleHwnd(), 0,400, 1280,500, true); // move window, TODO: make this
		// adjustable from the debugging window
	}
	else
	{
		CloseConsole();
	}
}
// ==============


void CDebugger::NotifyUpdate()
{
	if (m_GPRListView != NULL)
	{
		m_GPRListView->Update();
	}
}
