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
#include "../Logging/Console.h" // open and close console

// externals
extern int gSaveFile; // make this an int to allow multiple save file options
extern int gUpdFreq;
extern int gPreset;
extern bool gReset;
extern bool gSSBM;
extern bool gSSBMremedy1;
extern bool gSSBMremedy2;
extern bool gSequenced;
extern bool gVolume;
extern bool gOnlyLooping;

// =======================================================================================
// Declare events
BEGIN_EVENT_TABLE(CDebugger,wxDialog)	
	EVT_CLOSE(CDebugger::OnClose)
	EVT_BUTTON(ID_UPD,CDebugger::OnUpdate)
	EVT_CHECKBOX(IDC_CHECK0,CDebugger::SaveFile)
	EVT_CHECKBOX(IDC_CHECK2,CDebugger::ShowHideConsole)

	EVT_CHECKBOX(IDC_CHECK3,CDebugger::SSBM)
	EVT_CHECKBOX(IDC_CHECK4,CDebugger::SSBMremedy1)
	EVT_CHECKBOX(IDC_CHECK5,CDebugger::SSBMremedy2)
	EVT_CHECKBOX(IDC_CHECK8,CDebugger::Sequenced)
	EVT_CHECKBOX(IDC_CHECK9,CDebugger::Volume)
	EVT_CHECKBOX(IDC_CHECK6,CDebugger::Reset)
	EVT_CHECKBOX(IDC_CHECK7,CDebugger::OnlyLooping)

	EVT_RADIOBOX(IDC_RADIO1,CDebugger::ChangeFrequency)
	EVT_RADIOBOX(IDC_RADIO2,CDebugger::ChangePreset)
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
	// TODO2: get the screen resolution and make limits from that
	if(GetPosition().x < 1000 && GetPosition().y < 1000
		&& GetSize().GetWidth() < 1000 && GetSize().GetHeight() < 1000
		)
	{
		_IniFile.Set("SoundWindow", "x", GetPosition().x);
		_IniFile.Set("SoundWindow", "y", GetPosition().y);
		_IniFile.Set("SoundWindow", "w", GetSize().GetWidth());
		_IniFile.Set("SoundWindow", "h", GetSize().GetHeight());
	}
	_IniFile.Set("SoundWindow", "Console", m_Check[2]->IsChecked()); // save settings
	_IniFile.Set("SoundWindow", "UpdateFrequency", m_RadioBox[1]->GetSelection());
}


void CDebugger::Load(IniFile& _IniFile)
{
	int x,y,w,h;
	_IniFile.Get("SoundWindow", "x", &x, GetPosition().x);
	_IniFile.Get("SoundWindow", "y", &y, GetPosition().y);
	_IniFile.Get("SoundWindow", "w", &w, GetSize().GetWidth());
	_IniFile.Get("SoundWindow", "h", &h, GetSize().GetHeight());
	SetSize(x, y, w, h);

	// saved settings
	bool Console;
	_IniFile.Get("SoundWindow", "Console", &Console, m_Check[2]->IsChecked());
	m_Check[2]->SetValue(Console);
	DoShowHideConsole();

	int UpdateFrequency;
	_IniFile.Get("SoundWindow", "UpdateFrequency", &UpdateFrequency, m_RadioBox[1]->GetSelection());
	m_RadioBox[1]->SetSelection(UpdateFrequency);
	DoChangeFrequency();
}

void CDebugger::CreateGUIControls()
{
SetTitle(wxT("Sound Debugging"));

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
	wxButton* m_SelC;
	wxButton* m_Presets;

	wxStaticBoxSizer* sLeft;

	// checkboxes and labels -----------------------------------------------------
	m_Label[0] = new wxStaticBox(this, IDG_LABEL1, wxT("Options"),
		wxDefaultPosition, wxDefaultSize, 0);
	wxStaticBoxSizer * m_checkSizer = new wxStaticBoxSizer (m_Label[0], wxVERTICAL);

	// checkboxes
	m_Check[0] = new wxCheckBox(this, IDC_CHECK0, wxT("Save to file"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Check[1] = new wxCheckBox(this, IDC_CHECK1, wxT("Show updated"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Check[1]->Enable(false);
	m_Check[7] = new wxCheckBox(this, IDC_CHECK7, wxT("Only looping"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
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
	m_Check[3] = new wxCheckBox(this, IDC_CHECK3, wxT("SSBM fix"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
		m_Check[3]->SetValue(gSSBM);
	m_Check[4] = new wxCheckBox(this, IDC_CHECK4, wxT("SSBM remedy 1"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
		m_Check[4]->SetValue(gSSBMremedy1);
	m_Check[5] = new wxCheckBox(this, IDC_CHECK5, wxT("SSBM remedy 2"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
		m_Check[5]->SetValue(gSSBMremedy2);
	m_Check[8] = new wxCheckBox(this, IDC_CHECK8, wxT("Sequenced"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
		m_Check[8]->SetValue(gSequenced);
	m_Check[9] = new wxCheckBox(this, IDC_CHECK9, wxT("Volume delta"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
		m_Check[9]->SetValue(gVolume);
	m_Check[6] = new wxCheckBox(this, IDC_CHECK6, wxT("Reset all"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
		m_Check[6]->SetValue(gReset);
	
	m_checkSizer2->Add(m_Check[3], 0, 0, 5);
	m_checkSizer2->Add(m_Check[4], 0, 0, 5);
	m_checkSizer2->Add(m_Check[5], 0, 0, 5);
	m_checkSizer2->Add(m_Check[8], 0, 0, 5);
	m_checkSizer2->Add(m_Check[9], 0, 0, 5);
	m_checkSizer2->Add(m_Check[6], 0, 0, 5);
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

	wxString m_radioBoxChoices2[] = { wxT("Preset 1"), wxT("Updates"), wxT("Looping"), wxT("Mixer") };
	m_radioBoxNChoices[2] = sizeof( m_radioBoxChoices2 ) / sizeof( wxString );
	m_RadioBox[2] = new wxRadioBox( this, IDC_RADIO2, wxT("Presets"),
		wxDefaultPosition, wxDefaultSize, m_radioBoxNChoices[2], m_radioBoxChoices2, 1, wxRA_SPECIFY_COLS);
	// ------------------------

	// buttons
	m_Upd = new wxButton(this, ID_UPD, wxT("Update"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_SelC = new wxButton(this, ID_SELC, wxT("Select Columns"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_SelC->Enable(false);
	m_Presets = new wxButton(this, ID_PRESETS, wxT("Presets"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Presets->Enable(false);

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
	sButtons->Add(m_SelC, 0, 0, 5);
	sButtons->Add(m_Presets, 0, 0, 5);

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

	NotifyUpdate();
}

void CDebugger::OnClose(wxCloseEvent& /*event*/)
{	
	// save the window position when we hide the window to
	IniFile file;
	file.Load("Debugger.ini");
	this->Save(file);
	file.Save("Debugger.ini");

	EndModal(0);

	// I turned this off for now because of the ShowModal() problem and because I wanted
	// to look at the logs at the same time as the console window.
	//CloseConsole();
}

void CDebugger::OnUpdate(wxCommandEvent& /*event*/)
{
	this->NotifyUpdate();
}


// =======================================================================================
// Settings
// --------------
void CDebugger::SSBM(wxCommandEvent& event)
{
	if(m_Check[3]->IsChecked() == 1)
		{gSSBM = true;}
	else
		{gSSBM = false;}
}

void CDebugger::SSBMremedy1(wxCommandEvent& event)
{
	if(m_Check[4]->IsChecked() == 1)
		{gSSBMremedy1 = true;}
	else
		{gSSBMremedy1 = false;}
}
void CDebugger::SSBMremedy2(wxCommandEvent& event)
{
	if(m_Check[5]->IsChecked() == 1)
		{gSSBMremedy2 = true;}
	else
		{gSSBMremedy2 = false;}
}
void CDebugger::Sequenced(wxCommandEvent& event)
{
	if(m_Check[8]->IsChecked() == 1)
		{gSequenced = true;}
	else
		{gSequenced = false;}
}
void CDebugger::Volume(wxCommandEvent& event)
{
	if(m_Check[9]->IsChecked() == 1)
		{gVolume = true;}
	else
		{gVolume = false;}
}
void CDebugger::Reset(wxCommandEvent& event)
{
	if(m_Check[6]->IsChecked() == 1)
		{gReset = true;}
	else
		{gReset = false;}
}
// =======================================================================================


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
// Show only looping blocks
// --------------
void CDebugger::OnlyLooping(wxCommandEvent& event)
{	
	if(m_Check[7]->IsChecked())
	{
		gOnlyLooping = true;

	}
	else
	{
		gOnlyLooping = false;
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
