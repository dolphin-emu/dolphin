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


#include "../Globals.h" // The precompiled header

#include "IniFile.h" // Common
#include "ConsoleWindow.h" // Move console window

#include "../Config.h" // Config settings

#include "PBView.h" // Debugger files
#include "Debugger.h"
#include "Logging.h" // Open and close console


extern int gPreset;
int A, B;


// Event table and class
BEGIN_EVENT_TABLE(CDebugger,wxDialog)
	EVT_SHOW(CDebugger::OnShow)
	EVT_CLOSE(CDebugger::OnClose)
	EVT_BUTTON(ID_UPD,CDebugger::OnUpdate)

	EVT_CHECKBOX(ID_SAVETOFILE,CDebugger::GeneralSettings) // General settings
	EVT_CHECKBOX(ID_SHOWCONSOLE,CDebugger::GeneralSettings)
	EVT_CHECKLISTBOX(ID_CHECKLIST1, CDebugger::LogSettings) // Check list box
	EVT_RADIOBOX(IDC_RADIO1, CDebugger::ChangeFrequency) // Update freq.

	EVT_BUTTON(ID_AP,CDebugger::Ap)
	EVT_BUTTON(ID_AM,CDebugger::Am)
	EVT_BUTTON(ID_BP,CDebugger::Bp)
	EVT_BUTTON(ID_BM,CDebugger::Bm)
END_EVENT_TABLE()


CDebugger::CDebugger(wxWindow *parent, wxWindowID id, const wxString &title,
				   const wxPoint &position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
	, m_GPRListView(NULL)
{
	CreateGUIControls();

	// load ini...
	IniFile file;
	file.Load(DEBUGGER_CONFIG_FILE);
	this->Load(file);
}

CDebugger::~CDebugger()
{
	// empty
	IniFile file;
	file.Load(DEBUGGER_CONFIG_FILE);
	this->Save(file);
	file.Save(DEBUGGER_CONFIG_FILE);
} 


// System functions
void CDebugger::OnShow(wxShowEvent& /*event*/)
{	
	// bring the console back to
	if(m_Check[2]->IsChecked())
	{
		OpenConsole();
		#ifdef _WIN32
		MoveWindow(Console::GetHwnd(), 0,400, 1280,500, true); // Move window  TODO: make this
		// adjustable from the debugging window
		#endif
	}
}

void CDebugger::OnClose(wxCloseEvent& /*event*/)
{
	// save the window position when we hide the window to
	IniFile file;
	file.Load(DEBUGGER_CONFIG_FILE);
	this->Save(file);
	file.Save(DEBUGGER_CONFIG_FILE);

	EndModal(0); // it seems like this works for Show() to, not just ShowModal();
	CloseConsole(); // The console goes with the wx window
}


void CDebugger::DoHide()
{
	Hide();
	CloseConsole(); // The console goes with the wx window
}

void CDebugger::DoShow()
{
	Show();
	DoShowHideConsole(); // The console goes with the wx window
}


void CDebugger::OnUpdate(wxCommandEvent& /*event*/)
{
	this->NotifyUpdate();
}

// Save and load settings
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
	_IniFile.Set("VideoWindow", "Console", m_Check[2]->IsChecked()); // Save settings
	_IniFile.Set("VideoWindow", "WriteToFile", m_Check[0]->IsChecked());
	_IniFile.Set("VideoWindow", "UpdateFrequency", m_RadioBox[1]->GetSelection());
	_IniFile.Set("VideoWindow", "LogLevel", g_Config.iLog);
}


void CDebugger::Load(IniFile& _IniFile)
{
	int x,y,w,h;
	_IniFile.Get("VideoWindow", "x", &x, GetPosition().x);
	_IniFile.Get("VideoWindow", "y", &y, GetPosition().y);
	_IniFile.Get("VideoWindow", "w", &w, GetSize().GetWidth());
	_IniFile.Get("VideoWindow", "h", &h, GetSize().GetHeight());
	SetSize(x, y, w, h);

	// Saved settings
	bool Console;
	_IniFile.Get("VideoWindow", "Console", &Console, m_Check[2]->IsChecked());
	m_Check[2]->SetValue(Console);
	DoShowHideConsole();

	_IniFile.Get("VideoWindow", "WriteToFile", &LocalLogFile, m_Check[0]->IsChecked());
	 m_Check[0]->SetValue(LocalLogFile);

	_IniFile.Get("VideoWindow", "UpdateFrequency", &gUpdFreq, m_RadioBox[1]->GetSelection());
	m_RadioBox[1]->SetSelection(gUpdFreq);
	DoChangeFrequency();

	_IniFile.Get("VideoWindow", "LogLevel", &g_Config.iLog, 0);
	m_settings->Check(g_Config.iLog - 1, true);
}


void CDebugger::CreateGUIControls()
{
	// Basic settings
	SetTitle(wxT("OpenGL Debugging"));
	SetIcon(wxNullIcon);
	SetSize(8, 8, 200, 100); // these will become the minimin sizes allowed by resizing
	Center();

	// Declarations
	wxBoxSizer *sMain, *sGeneral;
	wxButton* m_Upd;
	wxButton* m_Ap; wxButton* m_Am;
	wxButton* m_Bp; wxButton* m_Bm;

	wxStaticBoxSizer* sLeft;


	// Notebook -----------------------------------------------------
	m_Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);
	m_PageMain = new wxPanel(m_Notebook, ID_PAGEMAIN, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PageMain, wxT("Main"));



	// Main Page


	// Buttons -----------------------------------------------------
	wxStaticBoxSizer * m_updSizer = new wxStaticBoxSizer (wxVERTICAL, m_PageMain, wxT("Update"));
	m_Upd = new wxButton(m_PageMain, ID_UPD, wxT("Update"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_updSizer->Add(m_Upd, 0, 0, 5);
	// ------------------------


	// Variables -----------------------------------------------------
	wxStaticBoxSizer * m_buttonSizer = new wxStaticBoxSizer (wxVERTICAL, m_PageMain, wxT("Variables"));
	m_Ap = new wxButton(m_PageMain, ID_AP, wxT("A +"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	//m_SelC->Enable(false);
	m_Am = new wxButton(m_PageMain, ID_AM, wxT("A -"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	//m_Presets->Enable(false);
	m_Bp = new wxButton(m_PageMain, ID_BP, wxT("B +"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Bm = new wxButton(m_PageMain, ID_BM, wxT("B -"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	m_buttonSizer->Add(m_Ap, 0, 0, 5);
	m_buttonSizer->Add(m_Am, 0, 0, 5);
	m_buttonSizer->Add(m_Bp, 0, 0, 5);
	m_buttonSizer->Add(m_Bm, 0, 0, 5);

	// m_PageMain: Options
	wxStaticBoxSizer * m_optionsSizer = new wxStaticBoxSizer(wxVERTICAL, m_PageMain, wxT("Options"));
	//m_Label[0] = new wxStaticBox(m_PageMain, IDG_LABEL1, wxT("Options"),
	//	wxDefaultPosition, wxDefaultSize, 0);
	//wxStaticBoxSizer * m_checkSizer3 = new wxStaticBoxSizer (m_Label[0], wxVERTICAL);

	// checkboxes
	m_Check[0] = new wxCheckBox(m_PageMain, ID_SAVETOFILE, wxT("Save to file"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Check[0]->SetToolTip(wxT("This will write the console output to" FULL_LOGS_DIR "oglgfx.txt"));

	m_Check[2] = new wxCheckBox(m_PageMain, ID_SHOWCONSOLE, wxT("Show console"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	
    m_optionsSizer->Add(m_Check[0], 0, 0, 5);
	m_optionsSizer->Add(m_Check[2], 0, 0, 5);	


	// m_PageMain: Log settings checkboxes
	wxStaticBoxSizer * m_logSizer = new wxStaticBoxSizer(wxVERTICAL, m_PageMain, wxT("Log setting"));
	m_settings = new wxCheckListBox(m_PageMain, ID_CHECKLIST1, wxDefaultPosition, wxDefaultSize,
		0, NULL, wxNO_BORDER);
	
	m_settings->Append(wxT("Info log"));
	m_settings->Append(wxT("Primary log"));

	m_settings->Check(0, bInfoLog);
	m_settings->Check(1, bPrimLog);

	// because the wxCheckListBox is a little underdeveloped we have to help it with this
	// to bad there's no windows xp styles for the checkboxes
	m_settings->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
	m_settings->SetMinSize(wxSize(m_settings->GetSize().GetWidth() - 40,
		m_settings->GetCount() * 15));
	
	m_logSizer->Add(m_settings, 0, 0, 0);
	// ------------------------


	// --------------------------------------------------------------------
	// m_PageMain: Radio boxes
	// -------------------------
	int m_radioBoxNChoices[3];

	wxString m_radioBoxChoices0[] = { wxT("Show base 10"), wxT("Show base 16") };
	m_radioBoxNChoices[0] = sizeof( m_radioBoxChoices0 ) / sizeof( wxString );
	m_RadioBox[0] = new wxRadioBox( m_PageMain, IDC_RADIO0, wxT("Show base"),
		wxDefaultPosition, wxDefaultSize, m_radioBoxNChoices[0], m_radioBoxChoices0, 1, wxRA_SPECIFY_COLS);
	m_RadioBox[0]->Enable(false);

	wxString m_radioBoxChoices1[] = { wxT("Never"), wxT("5 times/s"), wxT("15 times/s"), wxT("30 times/s") };
	m_radioBoxNChoices[1] = sizeof( m_radioBoxChoices1 ) / sizeof( wxString );
	m_RadioBox[1] = new wxRadioBox( m_PageMain, IDC_RADIO1, wxT("Update freq."),
		wxDefaultPosition, wxDefaultSize, m_radioBoxNChoices[1], m_radioBoxChoices1, 1, wxRA_SPECIFY_COLS);

	wxString m_radioBoxChoices2[] = { wxT("Win stretch") };
	m_radioBoxNChoices[2] = sizeof( m_radioBoxChoices2 ) / sizeof( wxString );
	m_RadioBox[2] = new wxRadioBox( m_PageMain, IDC_RADIO2, wxT("Presets"),
		wxDefaultPosition, wxDefaultSize, m_radioBoxNChoices[2], m_radioBoxChoices2, 1, wxRA_SPECIFY_COLS);
	// ------------------------


	// --------------------------------------------------------------------
	// Main: Left buttons and checkboxes
	// ------------------------
	wxBoxSizer* sButtons = new wxBoxSizer(wxVERTICAL);

	//sButtons->AddStretchSpacer(1);

	sButtons->Add(m_updSizer, 0, 0, 5);  // update button
	sButtons->Add(m_buttonSizer, 0, 0, 5); // variables buttons
	sButtons->Add(m_logSizer, 0, 0, 5); // log settings

	sButtons->Add(m_optionsSizer, 0, 2, 5); // Log options, show console etc.


	// --------------------------------------------------------------------
	// Main: Right buttons and checkboxes
	// ------------------------
	wxBoxSizer* sButtons2 = new wxBoxSizer(wxVERTICAL);

	sButtons2->Add(m_RadioBox[0], 0, 0, 5); // Show base
	sButtons2->Add(m_RadioBox[1], 0, 0, 5); // Update frequency
	sButtons2->Add(m_RadioBox[2], 0, 0, 5); // Preset views
	//sButtons2->AddStretchSpacer(1);
	//sButtons2->Add(m_checkSizer2, 0, 2, 5);


	// --------------------------------------------------------------------
	// Main: Parameter tables view, the big window
	sLeft = new wxStaticBoxSizer(wxVERTICAL, m_PageMain, wxT("Current Status"));

	m_GPRListView = new CPBView(m_PageMain, ID_GPR, wxDefaultPosition, GetSize(),
		wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL | wxLC_SORT_ASCENDING);

	sLeft->Add(m_GPRListView, 1, wxEXPAND|wxALL, 5);	


	// --------------------------------------------------------------------
	// General container
	// -----------------------------
	sGeneral = new wxBoxSizer(wxHORIZONTAL);
	sGeneral->Add(sLeft, 1, wxEXPAND | wxALL, 5);
	sGeneral->Add(sButtons, 0, wxEXPAND | (wxUP | wxDOWN), 5);
	sGeneral->Add(sButtons2, 0, wxEXPAND | (wxUP | wxDOWN | wxRIGHT | wxLEFT), 5);


	// --------------------------------------------------------------------
	// Main container
	// -----------------------------
	sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(m_Notebook, 1, wxEXPAND | wxALL, 5);

	m_PageMain->SetSizer(sGeneral);
	this->SetSizer(sMain);
	//sGeneral->SetSizeHints(this);

	//NotifyUpdate();
	//Freeze(); // unfreeze this if you want to use it
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
		gPreset = 0;
	else if(m_RadioBox[2]->GetSelection() == 1)
		gPreset = 1;
	else if(m_RadioBox[2]->GetSelection() == 2)
		gPreset = 2;
	else if(m_RadioBox[2]->GetSelection() == 3)
		gPreset = 3;
}
// ==============


// =======================================================================================
// Control variables
// --------------
void CDebugger::Ap(wxCommandEvent& event)
{
	A += 50;
	//MessageBox(0, "", "", 0);
	DEBUG_LOG("%i", A);
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
		gUpdFreq = 0;
	else if(m_RadioBox[1]->GetSelection() == 1)
		gUpdFreq = 5;
	else if(m_RadioBox[1]->GetSelection() == 2)
		gUpdFreq = 15;
	else
		gUpdFreq = 30;
}
// ==============


// =======================================================================================
// General settings
// --------------
void CDebugger::GeneralSettings(wxCommandEvent& event)
{
	switch (event.GetId())
	{
		case ID_SAVETOFILE: // Save to file
			LocalLogFile = m_Check[0]->IsChecked();
			break;
		case ID_SHOWCONSOLE:
			DoShowHideConsole();
			break;
	}
}
// ==============


// =======================================================================================
// Show or hide console window
// --------------
void CDebugger::DoShowHideConsole()
{
	if(m_Check[2]->IsChecked())
	{
		OpenConsole();
		#ifdef _WIN32
		MoveWindow(Console::GetHwnd(), 0,400, 1280,500, true); // Move window. TODO: make this
			// adjustable from the debugging window
		#endif
	}
	else
	{
		CloseConsole();
	}
}
// ==============


// =======================================================================================
// Enable or disable logs
// --------------
void CDebugger::LogSettings(wxCommandEvent& event)
{
	// Only allow one selected log at a time
	for (u32 i = 0; i < m_settings->GetCount(); ++i)
          if(i != (u32)event.GetInt()) m_settings->Check(i, false);

	if(m_settings->IsChecked(0)) g_Config.iLog = CONF_LOG;
	else if(m_settings->IsChecked(1)) g_Config.iLog = CONF_PRIMLOG;
	else g_Config.iLog = 0;
}
// ==============


// =======================================================================================
// Update the wxListCtrl
// --------------
void CDebugger::NotifyUpdate()
{
	if (m_GPRListView != NULL)
	{
		m_GPRListView->Update();
	}
}
// ==============
