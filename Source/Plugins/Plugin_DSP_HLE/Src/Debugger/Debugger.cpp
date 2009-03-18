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


//////////////////////////////////////////////////////////////////////////////////////////
// Includes
// -------------
// includes
#include <iostream> // System
#include <fstream>
#include <sstream>
#ifndef _WIN32
	#include <stdlib.h>
#endif

#include "Debugger.h"
#include "PBView.h"
#include "IniFile.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "FileSearch.h"
#include "../Debugger/File.h" // Write to file
//////////////////////////////

// =======================================================================================
// Event table and class
BEGIN_EVENT_TABLE(CDebugger,wxDialog)	
	EVT_CLOSE(CDebugger::OnClose) // on close event

	EVT_BUTTON(ID_UPD,CDebugger::OnUpdate) // buttons

	// left cotrols
	EVT_CHECKLISTBOX(IDC_CHECKLIST5, CDebugger::OnOptions) // options
	EVT_CHECKLISTBOX(IDC_CHECKLIST6, CDebugger::OnShowAll)
	EVT_RADIOBOX(IDC_RADIO0,CDebugger::ShowBase) // update frequency

	// right cotrols
	EVT_RADIOBOX(IDC_RADIO0,CDebugger::ShowBase)
	EVT_RADIOBOX(IDC_RADIO1,CDebugger::ChangeFrequency) // update frequency
	EVT_RADIOBOX(IDC_RADIO2,CDebugger::ChangePreset) // presets
	EVT_CHECKLISTBOX(IDC_CHECKLIST1, CDebugger::OnSettingsCheck) // settings

	EVT_NOTEBOOK_PAGE_CHANGED(ID_NOTEBOOK, CDebugger::UpdateMail) // mails
	EVT_RADIOBOX(IDC_RADIO3,CDebugger::ChangeMail)
	EVT_CHECKLISTBOX(IDC_CHECKLIST2, CDebugger::OnGameChange) // gc
	EVT_CHECKLISTBOX(IDC_CHECKLIST3, CDebugger::OnGameChange) // wii
	EVT_CHECKLISTBOX(IDC_CHECKLIST4, CDebugger::MailSettings) // settings

	//EVT_RIGHT_DOWN(CDebugger::ScrollBlocks)
	//EVT_LEFT_DOWN(CDebugger::ScrollBlocks)
	//EVT_MOUSE_EVENTS(CDebugger::ScrollBlocks)
	//EVT_MOTION(CDebugger::ScrollBlocks)

	//EVT_SCROLL(CDebugger::ScrollBlocks)
	//EVT_SCROLLWIN(CDebugger::ScrollBlocks)
END_EVENT_TABLE()

CDebugger::CDebugger(wxWindow *parent, wxWindowID id, const wxString &title,
				   const wxPoint &position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
	, m_GPRListView(NULL)
	//, gUpdFreq(5) // loaded from file
	, giShowAll(-1)
	, gPreset(0)


	, upd95(false) // block view settings
	, upd94(false)
	, upd93(false)
	, upd92(false)
{
	CreateGUIControls();

	// load ini...
	IniFile file;
	file.Load(DEBUGGER_CONFIG_FILE);
	this->Load(file);

	// append block names
	PBn.resize(266/2);
	PBn[10] = "mixer";
	PBn[34] = "initial_time_delay";
	PBn[41] = "updates";
	PBn[46] = "dpop";
	PBn[58] = "vol_env";
	PBn[60] = "audio_addr";
	PBn[68] = "adpcm";
	PBn[88] = "src";
	PBn[95] = "adpcm_loop_info";
	PBn[98] = "lpf";
	PBn[102] = "hpf";
	PBn[106] = "pad";

	PBp.resize(266/2);
	PBp[10] = "volume_left, unknown";

	PBp[58] = "cur_volume, cur_volume_delta"; // PBVolumeEnvelope
	

	PBp[60] = "looping, sample_format"; // PBAudioAddr
	PBp[62] = "loop_addr_hi, loop_addr_lo";
	PBp[64] = "end_addr_hi, end_addr_lo";
	PBp[66] = "cur_addr_hi, cur_addr_lo";

	PBp[68] = "coef[0], coef[1]"; // PBADPCMInfo

	PBp[94] = "cur_addr_frac, last_samples[0]";

	PBp[94] = "last_samples[3], pred_scale";
	PBp[96] = "yn1, yn2";

	//wxEVT_RIGHT_DOWN, wxEVT_MOUSEWHEEL, wxEVT_LEFT_UP, 
	//m_bl95, m_PageBlock, sBlock

	m_bl95->Connect(wxID_ANY, wxEVT_SCROLLWIN_THUMBTRACK,
		wxScrollWinEventHandler(CDebugger::ScrollBlocksCursor), (wxObject*)NULL, this);
	m_bl95->Connect(wxID_ANY, wxEVT_SCROLLWIN_THUMBRELEASE,
		wxScrollWinEventHandler(CDebugger::ScrollBlocksCursor), (wxObject*)NULL, this);
	m_bl95->Connect(wxID_ANY, wxEVT_MOTION,
		wxMouseEventHandler(CDebugger::ScrollBlocksMouse), (wxObject*)NULL, this);
	m_bl95->Connect(wxID_ANY, wxEVT_MOUSEWHEEL,
		wxMouseEventHandler(CDebugger::ScrollBlocksMouse), (wxObject*)NULL, this);			
}

CDebugger::~CDebugger()
{
	// empty
	IniFile file;
	file.Load(DEBUGGER_CONFIG_FILE);
	this->Save(file);
	file.Save(DEBUGGER_CONFIG_FILE);
} 
// ====================


// ========================================================================
// System functions
// --------------
void CDebugger::OnClose(wxCloseEvent& /*event*/)
{	
	// Save the window position when we hide the window to
	IniFile file;
	file.Load(DEBUGGER_CONFIG_FILE);
	this->Save(file);
	file.Save(DEBUGGER_CONFIG_FILE);

	EndModal(0);
}

void CDebugger::DoHide()
{
	Hide();
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
// ===============


// ==========================================================================
// Save and load settings
// --------------
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
	_IniFile.Set("SoundWindow", "Console", m_options->IsChecked(3)); // save settings
	_IniFile.Set("SoundWindow", "UpdateFrequency", m_RadioBox[1]->GetSelection());
	_IniFile.Set("SoundWindow", "ScanMails", m_gcwiiset->IsChecked(0));
	_IniFile.Set("SoundWindow", "StoreMails", m_gcwiiset->IsChecked(1));
	_IniFile.Set("SoundWindow", "ShowBase", m_RadioBox[0]->GetSelection() ? false : true);
}


void CDebugger::Load(IniFile& _IniFile)
{
	int x,y,w,h;
	_IniFile.Get("SoundWindow", "x", &x, GetPosition().x);
	_IniFile.Get("SoundWindow", "y", &y, GetPosition().y);
	_IniFile.Get("SoundWindow", "w", &w, GetSize().GetWidth());
	_IniFile.Get("SoundWindow", "h", &h, GetSize().GetHeight());
	SetSize(x, y, w, h);

	// Show console or not
	bool Console;
	_IniFile.Get("SoundWindow", "Console", &Console, m_options->IsChecked(3));
	m_options->Check(3, Console);
	DoShowHideConsole();

	// Show number base
	_IniFile.Get("SoundWindow", "ShowBase", &bShowBase, !m_RadioBox[0]->GetSelection());
	m_RadioBox[0]->SetSelection(!bShowBase);

	_IniFile.Get("SoundWindow", "UpdateFrequency", &gUpdFreq, m_RadioBox[1]->GetSelection());
	m_RadioBox[1]->SetSelection(gUpdFreq);
	DoChangeFrequency();

	// Read and store mails on/off
	_IniFile.Get("SoundWindow", "ScanMails", &ScanMails, m_gcwiiset->IsChecked(0));
	m_gcwiiset->Check(0, ScanMails);
	_IniFile.Get("SoundWindow", "StoreMails", &StoreMails, m_gcwiiset->IsChecked(1));
	m_gcwiiset->Check(1, StoreMails);	
}
// ===================


//////////////////////////////////////////////////////////////////////////////////////////
// Create GUI controls
// -------------
void CDebugger::CreateGUIControls()
{
SetTitle(wxT("Sound Debugging"));

	// Basic settings
	SetIcon(wxNullIcon);
	SetSize(8, 8, 200, 100); // These will become the minimin sizes allowed by resizing
	Center();


	// Declarations	
	wxBoxSizer * m_MainSizer, * sMain, *_sMail, * sBlock;

	wxButton* m_Upd;
	wxButton* m_SelC;
	wxButton* m_Presets;

	wxStaticBoxSizer* sLeft;

	int m_radioBoxNChoices[3];


	// Notebook -----------------------------------------------------
	m_Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);
	m_PageMain = new wxPanel(m_Notebook, ID_PAGEMAIN, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PageMain, wxT("Main"));
	m_PageMail = new wxPanel(m_Notebook, ID_PAGEMAIL, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PageMail, wxT("Mail"));
	m_PageBlock = new wxPanel(m_Notebook, ID_PAGEBLOCK, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PageBlock, wxT("Blocks"));




	// ===================================================================
	// Blocks Page

	wxStaticBoxSizer * m_bl0Sizer = new wxStaticBoxSizer (wxVERTICAL, m_PageBlock, wxT("Block"));
	m_bl0 = new wxTextCtrl(m_PageBlock, ID_BL0, _T(""), wxDefaultPosition, wxSize(250, 120),
		wxTE_RICH | wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP | wxNO_BORDER);
	m_bl0Sizer->Add(m_bl0, 1, wxEXPAND | wxALL, 0);

	wxStaticBoxSizer * m_bl1Sizer = new wxStaticBoxSizer (wxVERTICAL, m_PageBlock, wxT("Block 95"));
	m_bl95 = new wxTextCtrl(m_PageBlock, ID_BL95, _T(""), wxDefaultPosition, wxSize(300, 120),
		wxTE_RICH | wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP | wxNO_BORDER);
	m_bl1Sizer->Add(m_bl95, 1, wxEXPAND | wxALL, 0);

	wxStaticBoxSizer * m_bl2Sizer = new wxStaticBoxSizer (wxVERTICAL, m_PageBlock, wxT("Block 94"));
	m_bl94 = new wxTextCtrl(m_PageBlock, ID_BL94, _T(""), wxDefaultPosition, wxSize(300, 120),
		wxTE_RICH | wxTE_MULTILINE | wxTE_DONTWRAP | wxNO_BORDER );
	m_bl2Sizer->Add(m_bl94, 1, wxEXPAND | wxALL, 0);





	// ===================================================================
	// Mail Page

	wxStaticBoxSizer * m_m1Sizer = new wxStaticBoxSizer (wxVERTICAL, m_PageMail, wxT("Entire mail"));
	m_log = new wxTextCtrl(m_PageMail, ID_LOG, _T(""), wxDefaultPosition, wxSize(175, 120),
		wxTE_RICH | wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP | wxNO_BORDER);
	m_m1Sizer->Add(m_log, 1, wxEXPAND | wxALL, 0);

	wxStaticBoxSizer * m_m2Sizer = new wxStaticBoxSizer (wxVERTICAL, m_PageMail, wxT("Logged mail"));
	m_log1 = new wxTextCtrl(m_PageMail, ID_LOG1, _T(""), wxDefaultPosition, wxSize(300, 120),
		wxTE_RICH | wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP | wxNO_BORDER );
	m_m2Sizer->Add(m_log1, 1, wxEXPAND | wxALL, 0);

	// Show different mails, make room for five mails, in usual circumstances it's two or three
	wxString m_radioBoxChoices3[] = { wxT("0"), wxT("1"), wxT("2"), wxT("3"), wxT("4") };
	m_radioBoxNChoices[3] = sizeof( m_radioBoxChoices3 ) / sizeof( wxString );
	m_RadioBox[3] = new wxRadioBox( m_PageMail, IDC_RADIO3, wxT("Show mail"),
		wxDefaultPosition, wxDefaultSize, m_radioBoxNChoices[3], m_radioBoxChoices3, 1, wxRA_SPECIFY_COLS);
	m_RadioBox[3]->Enable(false);

	// Games checkboxes (m_PageMail) -----------------------------
	wxStaticBoxSizer * m_gameSizer1 = new wxStaticBoxSizer(wxVERTICAL, m_PageMail, wxT("GC"));
	m_gc = new wxCheckListBox(m_PageMail, IDC_CHECKLIST2, wxDefaultPosition, wxDefaultSize,
		0, NULL, wxNO_BORDER | wxLB_SINGLE);
	m_gameSizer1->Add(m_gc, 1, wxEXPAND | wxALL, 0);

	wxStaticBoxSizer * m_gameSizer2 = new wxStaticBoxSizer(wxVERTICAL, m_PageMail, wxT("Wii"));
	m_wii = new wxCheckListBox(m_PageMail, IDC_CHECKLIST3, wxDefaultPosition, wxDefaultSize,
		0, NULL, wxNO_BORDER | wxLB_SINGLE);
	m_gameSizer2->Add(m_wii, 1, wxEXPAND | wxALL, 0);

	// Settings
	wxStaticBoxSizer * m_gameSizer3 = new wxStaticBoxSizer(wxVERTICAL, m_PageMail, wxT("Settings"));
	m_gcwiiset = new wxCheckListBox(m_PageMail, IDC_CHECKLIST4, wxDefaultPosition, wxDefaultSize,
		0, NULL, wxNO_BORDER | wxLB_SINGLE);
	m_gameSizer3->Add(m_gcwiiset, 0, 0, 0);
	m_gcwiiset->Append(wxT("Scan mails"));
	m_gcwiiset->Append(wxT("Store mails"));
	


	// ===================================================================
	// Main Page

	// Options checkboxlist (m_PageMain) -----------------------------------
	wxStaticBoxSizer * m_checkSizer = new wxStaticBoxSizer (wxVERTICAL, m_PageMain, wxT("Options"));
	m_options = new wxCheckListBox(m_PageMain, IDC_CHECKLIST5, wxDefaultPosition, wxDefaultSize,
		0, NULL, wxNO_BORDER);

	// checkboxes
	m_options->Append(wxT("Save to file"));
	m_options->Append(wxT("Only looping"));
	m_options->Append(wxT("Show all"));
	m_options->Append(wxT("Show console"));

	m_options->Check(0, gSaveFile);
	m_options->Check(1, gOnlyLooping);
	m_options->Check(2, gShowAll);
	m_options->Check(3, gSaveFile);

	m_options->SetMinSize(wxSize(m_options->GetSize().GetWidth() - 40,
	m_options->GetCount() * 15));

	m_checkSizer->Add(m_options, 0, 0, 0);
	// ------------------------

	
	// Options checkboxlist (m_PageMain) -----------------------------------
	wxStaticBoxSizer * m_showallSizer = new wxStaticBoxSizer (wxVERTICAL, m_PageMain, wxT("Show all"));
	m_opt_showall = new wxCheckListBox(m_PageMain, IDC_CHECKLIST6, wxDefaultPosition, wxDefaultSize,
		0, NULL, wxNO_BORDER);

	// checkboxes
	m_opt_showall->Append(wxT("Part 1"));
	m_opt_showall->Append(wxT("Part 2"));
	m_opt_showall->Append(wxT("Part 3"));
	m_opt_showall->Append(wxT("Part 4"));

	m_opt_showall->SetMinSize(wxSize(m_opt_showall->GetSize().GetWidth() - 40,
	m_opt_showall->GetCount() * 15));

	m_showallSizer->Add(m_opt_showall, 0, 0, 0);


	// Update frequency, numeric base, presets radio boxes --------------------
	wxString m_radioBoxChoices0[] = { wxT("Show base 10"), wxT("Show base 16") };
	m_radioBoxNChoices[0] = sizeof( m_radioBoxChoices0 ) / sizeof( wxString );
	m_RadioBox[0] = new wxRadioBox( m_PageMain, IDC_RADIO0, wxT("Show base"),
		wxDefaultPosition, wxDefaultSize, m_radioBoxNChoices[0], m_radioBoxChoices0, 1, wxRA_SPECIFY_COLS);

	wxString m_radioBoxChoices1[] = { wxT("Never"), wxT("5 times/s"), wxT("15 times/s"), wxT("30 times/s") };
	m_radioBoxNChoices[1] = sizeof( m_radioBoxChoices1 ) / sizeof( wxString );
	m_RadioBox[1] = new wxRadioBox( m_PageMain, IDC_RADIO1, wxT("Update freq."),
		wxDefaultPosition, wxDefaultSize, m_radioBoxNChoices[1], m_radioBoxChoices1, 1, wxRA_SPECIFY_COLS);

	wxString m_radioBoxChoices2[] = { wxT("Preset 1"), wxT("Updates"), wxT("Looping"), wxT("Mixer") };
	m_radioBoxNChoices[2] = sizeof( m_radioBoxChoices2 ) / sizeof( wxString );
	m_RadioBox[2] = new wxRadioBox( m_PageMain, IDC_RADIO2, wxT("Presets"),
		wxDefaultPosition, wxDefaultSize, m_radioBoxNChoices[2], m_radioBoxChoices2, 1, wxRA_SPECIFY_COLS);
	// ------------------------


	// --------------------------------------------------------------------
	// Settings checkboxes (m_PageMain)
	// -------------------------
	wxStaticBoxSizer * m_checkSizer2 = new wxStaticBoxSizer(wxVERTICAL, m_PageMain, wxT("Settings"));
	m_settings = new wxCheckListBox(m_PageMain, IDC_CHECKLIST1, wxDefaultPosition, wxDefaultSize,
		0, NULL, wxNO_BORDER);
	
	m_settings->Append(wxT("SSBM fix"));
	m_settings->Append(wxT("SSBM remedy 1"));
	m_settings->Append(wxT("SSBM remedy 2"));
	m_settings->Append(wxT("Sequenced"));
	m_settings->Append(wxT("Volume delta"));
	m_settings->Append(wxT("Reset all"));
	
	m_settings->Check(0, gSSBM);
	m_settings->Check(1, gSSBMremedy1);
	m_settings->Check(2, gSSBMremedy2);
	m_settings->Check(3, gSequenced);
	m_settings->Check(4, gVolume);
	m_settings->Check(5, gReset);

	// because the wxCheckListBox is a little underdeveloped we have to help it with this
	// to bad there's no windows xp styles for the checkboxes
	m_settings->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
	m_settings->SetMinSize(wxSize(m_settings->GetSize().GetWidth() - 40,
		m_settings->GetCount() * 15));
#ifdef _WIN32
	//for (int i = 0; i < m_settings->GetCount(); ++i)
	//	m_settings->GetItem(i)->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
#endif
	m_checkSizer2->Add(m_settings, 0, 0, 0);
	// ------------------------


	// --------------------------------------------------------------------
	// Buttons
	// ------------------------
	m_Upd = new wxButton(m_PageMain, ID_UPD, wxT("Update"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_SelC = new wxButton(m_PageMain, ID_SELC, wxT("Select Columns"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_SelC->Enable(false);
	m_Presets = new wxButton(m_PageMain, ID_PRESETS, wxT("Presets"),
		wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Presets->Enable(false);
	// --------------------------------------------------------------------


	// --------------------------------------------------------------------
	// Left buttons and checkboxes (MAIN
	// ------------------------
	wxBoxSizer* sButtons;
	sButtons = new wxBoxSizer(wxVERTICAL);

	sButtons->AddSpacer(5); // to set a minimum margin

	sButtons->Add(m_Upd, 0, 0, 5);
	sButtons->Add(m_SelC, 0, 0, 5);
	sButtons->Add(m_Presets, 0, 0, 5);

	sButtons->AddStretchSpacer(1);
	sButtons->Add(m_checkSizer, 0, 0, 5);
	sButtons->AddStretchSpacer(1);
	sButtons->Add(m_showallSizer, 0, 0, 5);
	sButtons->AddStretchSpacer(1);
	sButtons->Add(m_RadioBox[0], 0, 0, 5);

	sButtons->AddSpacer(5);
	// ------------------------


	// --------------------------------------------------------------------
	// Right buttons and checkboxes (MAIN)
	// ------------------------
	wxBoxSizer* sButtons2;
	sButtons2 = new wxBoxSizer(wxVERTICAL);

	sButtons2->AddStretchSpacer(1);
	sButtons2->Add(m_RadioBox[1], 0, 0, 5);  // Update freq.
	sButtons2->AddStretchSpacer(1);
	sButtons2->Add(m_RadioBox[2], 0, 0, 5);
	sButtons2->AddStretchSpacer(1);
	sButtons2->Add(m_checkSizer2, 0, 0, 5);
	sButtons2->AddStretchSpacer(1);
	// ------------------------


	// --------------------------------------------------------------------
	// Parameter tables view (MAIN)
	sLeft = new wxStaticBoxSizer(wxVERTICAL, m_PageMain, wxT("Current Status"));

	// The big window that holds the parameter tables
	m_GPRListView = new CPBView(m_PageMain, ID_GPR, wxDefaultPosition, GetSize(),
			wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL | wxLC_SORT_ASCENDING);

	sLeft->Add(m_GPRListView, 1, wxEXPAND|wxALL, 5);	

	sMain = new wxBoxSizer(wxHORIZONTAL);
	sMain->Add(sLeft, 1, wxEXPAND | wxALL, 5); // margin = 5
	sMain->Add(sButtons, 0, wxALL, 0);
	sMain->Add(sButtons2, 0, wxALL, 5); // margin = 5
	// --------------------------------------------------------------------


	// --------------------------------------------------------------------
	// Add all stuff to the mail container (MAIL)
	// -----------------------------
	// For the buttons on the right
	wxBoxSizer * sMailRight = new wxBoxSizer(wxVERTICAL);
	//wxStaticBoxSizer * sMailRight = new wxStaticBoxSizer(wxVERTICAL, m_PageMail, wxT("Current"));

	_sMail = new wxBoxSizer(wxHORIZONTAL);
	_sMail->Add(m_m1Sizer, 0, wxEXPAND | (wxUP | wxDOWN), 5); // margin = 5
	_sMail->Add(m_m2Sizer, 1, wxEXPAND | (wxUP | wxDOWN | wxLEFT), 5); // margin = 5
	_sMail->Add(sMailRight, 0, wxEXPAND | wxALL, 0); // margin = 0

	sMailRight->Add(m_RadioBox[3], 0, wxALL, 5); // margin = 5
	sMailRight->Add(m_gameSizer1, 1, wxEXPAND | wxALL, 5); // margin = 5
	sMailRight->Add(m_gameSizer2, 1, wxEXPAND | wxALL, 5); // margin = 5	
	sMailRight->Add(m_gameSizer3, 0, wxALL, 5); // margin = 5	
	// --------------------------------------------------------------------


	// --------------------------------------------------------------------
	// The blocks view container (BLOCKS)
	// -----------------------------
	// For the buttons on the right
	//wxBoxSizer * sMailRight = new wxBoxSizer(wxVERTICAL);
	//wxStaticBoxSizer * sMailRight = new wxStaticBoxSizer(wxVERTICAL, m_PageMail, wxT("Current"));

	sBlock = new wxBoxSizer(wxHORIZONTAL);	
	sBlock->Add(m_bl0Sizer, 0, wxEXPAND | (wxUP | wxDOWN), 5); // margin = 5
	sBlock->Add(m_bl1Sizer, 1, wxEXPAND | (wxUP | wxDOWN | wxLEFT), 5); // margin = 5
	sBlock->Add(m_bl2Sizer, 1, wxEXPAND | (wxUP | wxDOWN | wxRIGHT), 5); // margin = 5
	//sBlock->Add(sMailRight, 0, wxEXPAND | wxALL, 0); // margin = 0

	/*sMailRight->Add(m_RadioBox[3], 0, wxALL, 5); // margin = 5
	sMailRight->Add(m_gameSizer1, 1, wxEXPAND | wxALL, 5); // margin = 5
	sMailRight->Add(m_gameSizer2, 1, wxEXPAND | wxALL, 5); // margin = 5	
	sMailRight->Add(m_gameSizer3, 0, wxALL, 5); // margin = 5*/
	// --------------------------------------------------------------------


	// --------------------------------------------------------------------
	// Main containers
	// -----------------------------
	m_MainSizer = new wxBoxSizer(wxVERTICAL);
	m_MainSizer->Add(m_Notebook, 1, wxEXPAND | wxALL, 5);
	//m_MainSizer->SetSizeHints(this);

	m_PageMain->SetSizer(sMain);
	m_PageMail->SetSizer(_sMail);
	m_PageBlock->SetSizer(sBlock);
	//sMain->Layout();	

	this->SetSizer(m_MainSizer);
	//this->Layout();

	NotifyUpdate();
	// --------------------------------------------------------------------
}
/////////////////////////////


// =======================================================================================
// Settings
// --------------
void CDebugger::OnSettingsCheck(wxCommandEvent& event)
{
	gSSBM = m_settings->IsChecked(0);
	gSSBMremedy1 = m_settings->IsChecked(1);
	gSSBMremedy2 = m_settings->IsChecked(2);
	gSequenced = m_settings->IsChecked(3);
	gVolume = m_settings->IsChecked(4);
	gReset = m_settings->IsChecked(5);

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
// Show base
// --------------
void CDebugger::ShowBase(wxCommandEvent& event)
{
	if(m_RadioBox[0]->GetSelection() == 0)
		bShowBase = true;
	else if(m_RadioBox[0]->GetSelection() == 1)
		bShowBase = false;
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
	else if(m_RadioBox[1]->GetSelection() == 3)
		gUpdFreq = 30;
}
// ==============


// =======================================================================================
// Options
// --------------
void CDebugger::OnOptions(wxCommandEvent& event)
{
	gSaveFile = m_options->IsChecked(0);
	gOnlyLooping = m_options->IsChecked(1);
	gShowAll = m_options->IsChecked(2);
	gSaveFile = m_options->IsChecked(3);

	if(event.GetInt() == 3) DoShowHideConsole();
}

void CDebugger::OnShowAll(wxCommandEvent& event)
{
	/// Only allow one selection at a time
	for (u32 i = 0; i < m_opt_showall->GetCount(); ++i)
          if(i != (u32)event.GetInt()) m_opt_showall->Check(i, false);

	if(m_opt_showall->IsChecked(0)) giShowAll = 0;
	else if(m_opt_showall->IsChecked(1)) giShowAll = 1;
	else if(m_opt_showall->IsChecked(2)) giShowAll = 2;
	else if(m_opt_showall->IsChecked(3)) giShowAll = 3;
	else giShowAll = -1;
}

// --------------


// =======================================================================================
// Show or hide console window
// --------------
void CDebugger::ShowHideConsole(wxCommandEvent& event)
{
	DoShowHideConsole();
}

void CDebugger::DoShowHideConsole()
{
// #ifdef _WIN32
// 	if(m_options->IsChecked(3))
// 		OpenConsole();
// 	else
// 		CloseConsole();
// #endif
	PanicAlert("oh crap");
}
// ==============


void CDebugger::NotifyUpdate()
{
	if (m_GPRListView != NULL)
	{
		m_GPRListView->Update();
	}
}

