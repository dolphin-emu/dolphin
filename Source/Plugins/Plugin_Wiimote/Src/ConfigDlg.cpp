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


/////////////////////////////////////////////////////////////////////////
// Include
// ------------
//#include "Common.h" // for u16
#include "CommonTypes.h" // for u16
#include "IniFile.h"
#include "Timer.h"

#include "wiimote_real.h" // Local
#include "wiimote_hid.h"
#include "main.h"
#include "ConfigDlg.h"
#include "Config.h"
#include "EmuMain.h" // for LoadRecordedMovements()
#include "EmuSubroutines.h" // for WmRequestStatus
/////////////////////////////


/////////////////////////////////////////////////////////////////////////
// Definitions
// ------------

/////////////////////////

/////////////////////////////////////////////////////////////////////////
// Event table
// ------------
BEGIN_EVENT_TABLE(ConfigDialog,wxDialog)
	EVT_CLOSE(ConfigDialog::OnClose)
	EVT_BUTTON(ID_CLOSE, ConfigDialog::CloseClick)
	EVT_BUTTON(ID_APPLY, ConfigDialog::CloseClick)
	EVT_BUTTON(ID_ABOUTOGL, ConfigDialog::AboutClick)

	EVT_CHECKBOX(ID_SIDEWAYSDPAD, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_WIDESCREEN, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_NUNCHUCKCONNECTED, ConfigDialog::GeneralSettingsChanged)	
	EVT_CHECKBOX(ID_CLASSICCONTROLLERCONNECTED, ConfigDialog::GeneralSettingsChanged)

	EVT_CHECKBOX(ID_CONNECT_REAL, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_USE_REAL, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_UPDATE_REAL, ConfigDialog::GeneralSettingsChanged)

	EVT_CHOICE(IDC_RECORD + 1, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 2, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 3, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 4, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 5, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 6, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 7, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 8, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 9, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 10, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 11, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 12, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 13, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 14, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 15, ConfigDialog::GeneralSettingsChanged)

	EVT_BUTTON(IDB_RECORD + 1, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 2, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 3, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 4, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 5, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 6, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 7, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 8, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 9, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 10, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 11, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 12, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 13, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 14, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 15, ConfigDialog::RecordMovement)

	EVT_TIMER(IDTM_UPDATE, ConfigDialog::Update)	
END_EVENT_TABLE()
/////////////////////////////


/////////////////////////////////////////////////////////////////////////
// Class
// ------------
ConfigDialog::ConfigDialog(wxWindow *parent, wxWindowID id, const wxString &title,
						   const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
	#if wxUSE_TIMER
		m_TimeoutTimer = new wxTimer(this, IDTM_UPDATE);
		m_TimeoutATimer = new wxTimer(this, IDTM_UPDATEA);
		// Reset values
		m_bWaitForRecording = false;
		m_bRecording = false;
	#endif

	ControlsCreated = false;
	m_vRecording.resize(RECORDING_ROWS + 1);

	g_Config.Load();
	CreateGUIControls();
	LoadFile();
	UpdateGUI();

	wxTheApp->Connect(wxID_ANY, wxEVT_KEY_DOWN, // Keyboard
		wxKeyEventHandler(ConfigDialog::OnKeyDown),
		(wxObject*)0, this);
}

ConfigDialog::~ConfigDialog()
{	
}

void ConfigDialog::OnKeyDown(wxKeyEvent& event)
{
	event.Skip();

	// Escape a recording event
	if (event.GetKeyCode() == WXK_ESCAPE)
	{
		m_bWaitForRecording = false;
		m_bRecording = false;
		UpdateGUI();
	}
}
void ConfigDialog::OnClose(wxCloseEvent& WXUNUSED (event))
{
	SaveFile();
	g_Config.Save();
	g_FrameOpen = false;	
	//SuccessAlert("Saved\n");
	if (!g_EmulatorRunning) Shutdown();
	EndModal(0);
}

void ConfigDialog::CloseClick(wxCommandEvent& event)
{
	switch(event.GetId())
	{
	case ID_CLOSE:
		// wxWidgets function. This will also trigger EVT_CLOSE().
		Close();
		break;
	case ID_APPLY:
		SaveFile();
		WiiMoteEmu::LoadRecordedMovements();
		break;
	}
}

void ConfigDialog::AboutClick(wxCommandEvent& WXUNUSED (event))
{
}

void ConfigDialog::LoadFile()
{
	Console::Print("LoadFile\n");

	IniFile file;
	file.Load("WiimoteMovement.ini");

	for(int i = 1; i < (RECORDING_ROWS + 1); i++)
	{
		// Get row name
		std::string SaveName = StringFromFormat("Recording%i", i);

		// HotKey
		int TmpRecordHotKey; file.Get(SaveName.c_str(), "HotKey", &TmpRecordHotKey, -1);
		m_RecordHotKey[i]->SetSelection(TmpRecordHotKey);

		// Movement name
		std::string TmpMovementName; file.Get(SaveName.c_str(), "MovementName", &TmpMovementName, "");
		m_RecordText[i]->SetValue(wxString::FromAscii(TmpMovementName.c_str()));

		// Game name
		std::string TmpGameName; file.Get(SaveName.c_str(), "GameName", &TmpGameName, "");
		m_RecordGameText[i]->SetValue(wxString::FromAscii(TmpGameName.c_str()));

		// IR Bytes
		std::string TmpIRBytes; file.Get(SaveName.c_str(), "IRBytes", &TmpIRBytes, "");
		m_RecordIRBytesText[i]->SetValue(wxString::FromAscii(TmpIRBytes.c_str()));

		// Recording speed
		int TmpRecordSpeed; file.Get(SaveName.c_str(), "RecordingSpeed", &TmpRecordSpeed, -1);
		if(TmpRecordSpeed != -1)
			m_RecordSpeed[i]->SetValue(wxString::Format(wxT("%i"), TmpRecordSpeed));
		else
			m_RecordSpeed[i]->SetValue(wxT(""));

		// Playback speed (currently always saved directly after a recording)
		int TmpPlaybackSpeed; file.Get(SaveName.c_str(), "PlaybackSpeed", &TmpPlaybackSpeed, -1);
		m_RecordPlayBackSpeed[i]->SetSelection(TmpPlaybackSpeed);
	}
}
void ConfigDialog::SaveFile()
{
	Console::Print("SaveFile\n");

	IniFile file;
	file.Load("WiimoteMovement.ini");

	for(int i = 1; i < (RECORDING_ROWS + 1); i++)
	{
		// Get row name
		std::string SaveName = StringFromFormat("Recording%i", i);

		// HotKey
		file.Set(SaveName.c_str(), "HotKey", m_RecordHotKey[i]->GetSelection());

		// Movement name
		file.Set(SaveName.c_str(), "MovementName", m_RecordText[i]->GetValue().c_str());

		// Game name
		file.Set(SaveName.c_str(), "GameName", m_RecordGameText[i]->GetValue().c_str());

		// Recording speed (currently always saved directly after a recording)
		/*
		wxString TmpRecordSpeed = m_RecordSpeed[i]->GetValue();		
		if(TmpRecordSpeed.length() > 0)			
			int TmpRecordSpeed; file.Set(SaveName.c_str(), "RecordingSpeed", TmpRecordSpeed);
		else
			int TmpRecordSpeed; file.Set(SaveName.c_str(), "RecordingSpeed", "-1");
		*/

		// Playback speed (currently always saved directly after a recording)
		file.Set(SaveName.c_str(), "PlaybackSpeed", m_RecordPlayBackSpeed[i]->GetSelection());
	}

	file.Save("WiimoteMovement.ini");
}
/////////////////////////////


/////////////////////////////////////////////////////////////////////////
// Create GUI
// ------------
void ConfigDialog::CreateGUIControls()
{
	// Notebook
	m_Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);
	m_PageEmu = new wxPanel(m_Notebook, ID_PAGEEMU, wxDefaultPosition, wxDefaultSize);
	m_PageReal = new wxPanel(m_Notebook, ID_PAGEREAL, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PageEmu, wxT("Emulated Wiimote"));
	m_Notebook->AddPage(m_PageReal, wxT("Real Wiimote"));

	// Buttons
	//m_About = new wxButton(this, ID_ABOUTOGL, wxT("About"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Apply = new wxButton(this, ID_APPLY, wxT("Apply"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Close = new wxButton(this, ID_CLOSE, wxT("Close"));
	m_Close->SetToolTip(wxT("Apply and Close"));

	////////////////////////////////////////////
	// Put notebook and buttons in sMain
	// ----------------	
	wxBoxSizer* sButtons;
	sButtons = new wxBoxSizer(wxHORIZONTAL);
	//sButtons->Add(m_About, 0, wxALL, 5); // there is no about
	sButtons->AddStretchSpacer();
	sButtons->Add(m_Apply, 0, (wxALL), 0);
	sButtons->Add(m_Close, 0, (wxLEFT), 5);	

	wxBoxSizer* sMain;
	sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(m_Notebook, 1, wxEXPAND | wxALL, 5);
	sMain->Add(sButtons, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
	/////////////////////////////////


	////////////////////////////////////////////
	// Emulated Wiimote
	// ----------------
	// General
	wxStaticBoxSizer * sEmulatedBasic = new wxStaticBoxSizer(wxVERTICAL, m_PageEmu, wxT("Basic Settings"));
	m_SidewaysDPad = new wxCheckBox(m_PageEmu, ID_SIDEWAYSDPAD, wxT("Sideways D-Pad"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_SidewaysDPad->SetValue(g_Config.bSidewaysDPad);
	m_WideScreen = new wxCheckBox(m_PageEmu, ID_WIDESCREEN, wxT("WideScreen Mode (for correct aiming)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_WideScreen->SetValue(g_Config.bWideScreen);
	m_NunchuckConnected = new wxCheckBox(m_PageEmu, ID_NUNCHUCKCONNECTED, wxT("Nunchuck connected"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_NunchuckConnected->SetValue(g_Config.bNunchuckConnected);
	m_ClassicControllerConnected = new wxCheckBox(m_PageEmu, ID_CLASSICCONTROLLERCONNECTED, wxT("Classic Controller connected"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_ClassicControllerConnected->SetValue(g_Config.bClassicControllerConnected);

	// ----------------------------------------------------------------------
	// Set up sGeneral and sBasic
	// Usage: The wxGBPosition() must have a column and row
	// ----------------
	wxBoxSizer * sEmulatedMain = new wxBoxSizer(wxVERTICAL);
	wxGridBagSizer * GbsBasic = new wxGridBagSizer(0, 0);
	GbsBasic->Add(m_SidewaysDPad, wxGBPosition(0, 0), wxGBSpan(1, 2), wxALL, 5);
	GbsBasic->Add(m_WideScreen, wxGBPosition(1, 0), wxGBSpan(1, 2), wxALL, 5);
	GbsBasic->Add(m_NunchuckConnected, wxGBPosition(2, 0), wxGBSpan(1, 2), wxALL, 5);
	GbsBasic->Add(m_ClassicControllerConnected, wxGBPosition(3, 0), wxGBSpan(1, 2), wxALL, 5);
	sEmulatedBasic->Add(GbsBasic);
	sEmulatedMain->Add(sEmulatedBasic, 0, wxEXPAND | (wxALL), 5);
	/////////////////////////////////


	////////////////////////////////////////////////////////////////////////////////
	// Real Wiimote
	// ----------------
	// Basic Settings
	wxStaticBoxSizer * sbRealBasic = new wxStaticBoxSizer(wxVERTICAL, m_PageReal, wxT("Basic Settings"));
	m_ConnectRealWiimote = new wxCheckBox(m_PageReal, ID_CONNECT_REAL, wxT("Connect real Wiimote"));
	m_UseRealWiimote = new wxCheckBox(m_PageReal, ID_USE_REAL, wxT("Use real Wiimote"));
	m_ConnectRealWiimote->SetToolTip(wxT("Connected to the real wiimote. This can not be changed during gameplay."));
	m_UseRealWiimote->SetToolTip(wxT("Use the real Wiimote in the game. This can be changed during gameplay."));
	m_ConnectRealWiimote->SetValue(g_Config.bConnectRealWiimote);
	m_UseRealWiimote->SetValue(g_Config.bUseRealWiimote);

	// Status
	wxStaticBoxSizer * sbRealStatus = new wxStaticBoxSizer(wxVERTICAL, m_PageReal, wxT("Status"));
	m_TextUpdateRate = new wxStaticText(m_PageReal, wxID_ANY, wxT("Update rate: 000 times/s"));
	m_UpdateMeters = new wxCheckBox(m_PageReal, ID_UPDATE_REAL, wxT("Update gauges"));

	m_UpdateMeters->SetValue(g_Config.bUpdateRealWiimote);

	m_UpdateMeters->SetToolTip(wxT(
		"You can turn this off when a game is running to avoid a potential slowdown that may come from redrawing the\n"
		"configuration screen. Remember that you also need to press '+' on your Wiimote before you can record movements."
		));

	sbRealStatus->Add(m_TextUpdateRate, 0, wxEXPAND | (wxALL), 5);
	sbRealStatus->Add(m_UpdateMeters, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxUP), 5);

	// ==================================================
	// Wiimote Status
	// ----------------
	wxBoxSizer * sbRealWiimoteStatus = new wxBoxSizer(wxHORIZONTAL);

	wxStaticBoxSizer * sbRealBattery = new wxStaticBoxSizer(wxVERTICAL, m_PageReal, wxT("Battery"));
	wxStaticBoxSizer * sbRealRoll = new wxStaticBoxSizer(wxHORIZONTAL, m_PageReal, wxT("Roll and Pitch"));
	wxStaticBoxSizer * sbRealGForce = new wxStaticBoxSizer(wxHORIZONTAL, m_PageReal, wxT("G-Force"));
	wxStaticBoxSizer * sbRealAccel = new wxStaticBoxSizer(wxHORIZONTAL, m_PageReal, wxT("Accelerometer"));
	wxStaticBoxSizer * sbRealIR = new wxStaticBoxSizer(wxHORIZONTAL, m_PageReal, wxT("IR"));

	// Width and height of the gauges
	static const int Gw = 35, Gh = 130;

	m_GaugeBattery = new wxGauge( m_PageReal, wxID_ANY, 100, wxDefaultPosition, wxSize(Gw, Gh), wxGA_VERTICAL | wxNO_BORDER | wxGA_SMOOTH);
	m_GaugeRoll[0] = new wxGauge( m_PageReal, wxID_ANY, 360, wxDefaultPosition, wxSize(Gw, Gh), wxGA_VERTICAL | wxNO_BORDER | wxGA_SMOOTH);
	m_GaugeRoll[1] = new wxGauge( m_PageReal, wxID_ANY, 360, wxDefaultPosition, wxSize(Gw, Gh), wxGA_VERTICAL | wxNO_BORDER | wxGA_SMOOTH);
	m_GaugeGForce[0] = new wxGauge( m_PageReal, wxID_ANY, 600, wxDefaultPosition, wxSize(Gw, Gh), wxGA_VERTICAL | wxNO_BORDER | wxGA_SMOOTH);
	m_GaugeGForce[1] = new wxGauge( m_PageReal, wxID_ANY, 600, wxDefaultPosition, wxSize(Gw, Gh), wxGA_VERTICAL | wxNO_BORDER | wxGA_SMOOTH);
	m_GaugeGForce[2] = new wxGauge( m_PageReal, wxID_ANY, 600, wxDefaultPosition, wxSize(Gw, Gh), wxGA_VERTICAL | wxNO_BORDER | wxGA_SMOOTH);
	m_GaugeAccel[0] = new wxGauge( m_PageReal, wxID_ANY, 255, wxDefaultPosition, wxSize(Gw, Gh), wxGA_VERTICAL | wxNO_BORDER | wxGA_SMOOTH);
	m_GaugeAccel[1] = new wxGauge( m_PageReal, wxID_ANY, 255, wxDefaultPosition, wxSize(Gw, Gh), wxGA_VERTICAL | wxNO_BORDER | wxGA_SMOOTH);
	m_GaugeAccel[2] = new wxGauge( m_PageReal, wxID_ANY, 255, wxDefaultPosition, wxSize(Gw, Gh), wxGA_VERTICAL | wxNO_BORDER | wxGA_SMOOTH);

	// The text controls
	m_TextIR = new wxStaticText(m_PageReal, wxID_ANY, wxT("Cursor: 000 000\nDistance: 0000"));

	// -----------------------------
	// The sizers for all gauges together with their label
	// -----------
	wxBoxSizer * sBoxBattery = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer * sBoxRoll[2];
	sBoxRoll[0] = new wxBoxSizer(wxVERTICAL);
	sBoxRoll[1] = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer * sBoxGForce[3];
	sBoxGForce[0] = new wxBoxSizer(wxVERTICAL);
	sBoxGForce[1] = new wxBoxSizer(wxVERTICAL);
	sBoxGForce[2] = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer * sBoxAccel[3];
	sBoxAccel[0] = new wxBoxSizer(wxVERTICAL);
	sBoxAccel[1] = new wxBoxSizer(wxVERTICAL);
	sBoxAccel[2] = new wxBoxSizer(wxVERTICAL);

	wxStaticText * m_TextBattery = new wxStaticText(m_PageReal, wxID_ANY, wxT("Batt."), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
	wxStaticText * m_TextRoll = new wxStaticText(m_PageReal, wxID_ANY, wxT("Roll"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
	wxStaticText * m_TextPitch = new wxStaticText(m_PageReal, wxID_ANY, wxT("Pitch"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
	wxStaticText *m_TextX[2], *m_TextY[2], *m_TextZ[2];
	m_TextX[0] = new wxStaticText(m_PageReal, wxID_ANY, wxT("X"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE); m_TextX[1] = new wxStaticText(m_PageReal, wxID_ANY, wxT("X"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
	m_TextY[0] = new wxStaticText(m_PageReal, wxID_ANY, wxT("Y"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE); m_TextY[1] = new wxStaticText(m_PageReal, wxID_ANY, wxT("Y"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
	m_TextZ[0] = new wxStaticText(m_PageReal, wxID_ANY, wxT("Z"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE); m_TextZ[1] = new wxStaticText(m_PageReal, wxID_ANY, wxT("Z"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
	// ----------------

	// -----------------------------
	// Set up sizers
	// -----------
	sBoxBattery->Add(m_GaugeBattery, 0, wxEXPAND | (wxALL), 5); sBoxBattery->Add(m_TextBattery, 0, wxEXPAND | (wxALL), 0);

	sBoxRoll[0]->Add(m_GaugeRoll[0], 0, wxEXPAND | (wxALL), 5); sBoxRoll[0]->Add(m_TextRoll, 0, wxEXPAND | (wxALL), 0);
	sBoxRoll[1]->Add(m_GaugeRoll[1], 0, wxEXPAND | (wxALL), 5); sBoxRoll[1]->Add(m_TextPitch, 0, wxEXPAND | (wxALL), 0);

	sBoxGForce[0]->Add(m_GaugeGForce[0], 0, wxEXPAND | (wxALL), 5); sBoxGForce[0]->Add(m_TextX[0], 0, wxEXPAND | (wxALL), 0);
	sBoxGForce[1]->Add(m_GaugeGForce[1], 0, wxEXPAND | (wxALL), 5); sBoxGForce[1]->Add(m_TextY[0], 0, wxEXPAND | (wxALL), 0);
	sBoxGForce[2]->Add(m_GaugeGForce[2], 0, wxEXPAND | (wxALL), 5); sBoxGForce[2]->Add(m_TextZ[0], 0, wxEXPAND | (wxALL), 0);

	sBoxAccel[0]->Add(m_GaugeAccel[0], 0, wxEXPAND | (wxALL), 5); sBoxAccel[0]->Add(m_TextX[1], 0, wxEXPAND | (wxALL), 0);
	sBoxAccel[1]->Add(m_GaugeAccel[1], 0, wxEXPAND | (wxALL), 5); sBoxAccel[1]->Add(m_TextY[1], 0, wxEXPAND | (wxALL), 0);
	sBoxAccel[2]->Add(m_GaugeAccel[2], 0, wxEXPAND | (wxALL), 5); sBoxAccel[2]->Add(m_TextZ[1], 0, wxEXPAND | (wxALL), 0);

	sbRealBattery->Add(sBoxBattery, 0, wxEXPAND | (wxALL), 5);
	sbRealRoll->Add(sBoxRoll[0], 0, wxEXPAND | (wxALL), 5); sbRealRoll->Add(sBoxRoll[1], 0, wxEXPAND | (wxALL), 5);
	sbRealGForce->Add(sBoxGForce[0], 0, wxEXPAND | (wxALL), 5); sbRealGForce->Add(sBoxGForce[1], 0, wxEXPAND | (wxALL), 5); sbRealGForce->Add(sBoxGForce[2], 0, wxEXPAND | (wxALL), 5);
	sbRealAccel->Add(sBoxAccel[0], 0, wxEXPAND | (wxALL), 5); sbRealAccel->Add(sBoxAccel[1], 0, wxEXPAND | (wxALL), 5); sbRealAccel->Add(sBoxAccel[2], 0, wxEXPAND | (wxALL), 5);
	sbRealIR->Add(m_TextIR, 0, wxEXPAND | (wxALL), 5);

	sbRealWiimoteStatus->Add(sbRealBattery, 0, wxEXPAND | (wxLEFT), 0);
	sbRealWiimoteStatus->Add(sbRealRoll, 0, wxEXPAND | (wxLEFT), 5);
	sbRealWiimoteStatus->Add(sbRealGForce, 0, wxEXPAND | (wxLEFT), 5);
	sbRealWiimoteStatus->Add(sbRealAccel, 0, wxEXPAND | (wxLEFT), 5);
	sbRealWiimoteStatus->Add(sbRealIR, 0, wxEXPAND | (wxLEFT), 5);
	// ----------------

	// Tool tips
	m_GaugeBattery->SetToolTip(wxT("Press '+' to show the current status. Press '-' to stop recording the status."));
	// ==========================================


	// ====================================================================
	// Record movement
	// ----------------
	wxStaticBoxSizer * sbRealRecord = new wxStaticBoxSizer(wxVERTICAL, m_PageReal, wxT("Record movements"));

	wxArrayString StrHotKey;
	for(int i = 0; i < 10; i++) StrHotKey.Add(wxString::Format(wxT("%i"), i));
	StrHotKey.Add(wxT(""));

	wxArrayString StrPlayBackSpeed;
	for(int i = 1; i <= 20; i++) StrPlayBackSpeed.Add(wxString::Format(wxT("%i"), i*25));

	wxBoxSizer * sRealRecord[RECORDING_ROWS + 1];

	wxStaticText * m_TextRec = new wxStaticText(m_PageReal, wxID_ANY, wxT("Rec."), wxDefaultPosition, wxSize(80, 15), wxALIGN_CENTRE);
	wxStaticText * m_TextHotKey = new wxStaticText(m_PageReal, wxID_ANY, wxT("HotKey"), wxDefaultPosition, wxSize(40, 15), wxALIGN_CENTRE);
	wxStaticText * m_TextMovement = new wxStaticText(m_PageReal, wxID_ANY, wxT("Movement name"), wxDefaultPosition, wxSize(200, 15), wxALIGN_CENTRE);
	wxStaticText * m_TextGame = new wxStaticText(m_PageReal, wxID_ANY, wxT("Game name"), wxDefaultPosition, wxSize(200, 15), wxALIGN_CENTRE);
	wxStaticText * m_TextIRBytes = new wxStaticText(m_PageReal, wxID_ANY, wxT("IR"), wxDefaultPosition, wxSize(20, 15), wxALIGN_CENTRE);
	wxStaticText * m_TextRecSped = new wxStaticText(m_PageReal, wxID_ANY, wxT("R. s."), wxDefaultPosition, wxSize(33, 15), wxALIGN_CENTRE);
	wxStaticText * m_TextPlaySpeed = new wxStaticText(m_PageReal, wxID_ANY, wxT("Pl. s."), wxDefaultPosition, wxSize(40, 15), wxALIGN_CENTRE);
	m_TextRec->SetToolTip(wxT(
		"To record a movement first press this button, then start the recording by pressing 'A' on the Wiimote and stop the recording\n"
		"by letting go of 'A'"));
	m_TextHotKey->SetToolTip(wxT("The HotKey is Shift + [Number] for Wiimote movements and Ctrl + [Number] for Nunchuck movements"));
	m_TextRecSped->SetToolTip(wxT("Recording speed in average measurements per second"));
	m_TextPlaySpeed->SetToolTip(wxT(
		"Playback speed: A playback speed of 100 means that the playback occurs at the same rate as it was recorded. (You can see the\n"
		"current update rate in the Status window above when a game is running.) However, if your framerate is only at 50% of full speed\n"
		"you may want to select a playback rate of 50, because then the game might perceive the playback as a full speed playback. (This\n"
		"holds if Wiimote_Update() is tied to the framerate, I'm not sure that this is the case. It seemed to vary somewhat with different\n"
		"framerates but perhaps not enough to say that it was exactly tied to the framerate.) So until this is better understood you'll have\n"
		"to try different playback rates and see which one that works."
		));

	sRealRecord[0] = new wxBoxSizer(wxHORIZONTAL);
	sRealRecord[0]->Add(m_TextRec, 0, wxEXPAND | (wxLEFT), 5);
	sRealRecord[0]->Add(m_TextHotKey, 0, wxEXPAND | (wxLEFT), 5);
	sRealRecord[0]->Add(m_TextMovement, 0, wxEXPAND | (wxLEFT), 5);
	sRealRecord[0]->Add(m_TextGame, 0, wxEXPAND | (wxLEFT), 5);
	sRealRecord[0]->Add(m_TextIRBytes, 0, wxEXPAND | (wxLEFT), 5);
	sRealRecord[0]->Add(m_TextRecSped, 0, wxEXPAND | (wxLEFT), 5);
	sRealRecord[0]->Add(m_TextPlaySpeed, 0, wxEXPAND | (wxLEFT), 5);
	sbRealRecord->Add(sRealRecord[0], 0, wxEXPAND | (wxALL), 0);

	for(int i = 1; i < (RECORDING_ROWS + 1); i++)
	{
		sRealRecord[i] = new wxBoxSizer(wxHORIZONTAL);
		m_RecordButton[i] = new wxButton(m_PageReal, IDB_RECORD + i, wxEmptyString, wxDefaultPosition, wxSize(80, 20), 0, wxDefaultValidator, wxEmptyString);
		m_RecordHotKey[i] = new wxChoice(m_PageReal, IDC_RECORD + i, wxDefaultPosition, wxDefaultSize, StrHotKey);
		m_RecordText[i] = new wxTextCtrl(m_PageReal, IDT_RECORD_TEXT, wxT(""), wxDefaultPosition, wxSize(200, 19));
		m_RecordGameText[i] = new wxTextCtrl(m_PageReal, IDT_RECORD_GAMETEXT, wxT(""), wxDefaultPosition, wxSize(200, 19));
		m_RecordIRBytesText[i] = new wxTextCtrl(m_PageReal, IDT_RECORD_IRBYTESTEXT, wxT(""), wxDefaultPosition, wxSize(25, 19));
		m_RecordSpeed[i] = new wxTextCtrl(m_PageReal, IDT_RECORD_SPEED, wxT(""), wxDefaultPosition, wxSize(30, 19), wxTE_READONLY | wxTE_CENTRE);
		m_RecordPlayBackSpeed[i] = new wxChoice(m_PageReal, IDT_RECORD_PLAYSPEED, wxDefaultPosition, wxDefaultSize, StrPlayBackSpeed);

		m_RecordText[i]->SetMaxLength(35);
		m_RecordGameText[i]->SetMaxLength(35);
		m_RecordIRBytesText[i]->Enable(false);
		m_RecordSpeed[i]->Enable(false);

		sRealRecord[i]->Add(m_RecordButton[i], 0, wxEXPAND | (wxLEFT), 5);
		sRealRecord[i]->Add(m_RecordHotKey[i], 0, wxEXPAND | (wxLEFT), 5);
		sRealRecord[i]->Add(m_RecordText[i], 0, wxEXPAND | (wxLEFT), 5);
		sRealRecord[i]->Add(m_RecordGameText[i], 0, wxEXPAND | (wxLEFT), 5);
		sRealRecord[i]->Add(m_RecordIRBytesText[i], 0, wxEXPAND | (wxLEFT), 5);
		sRealRecord[i]->Add(m_RecordSpeed[i], 0, wxEXPAND | (wxLEFT), 5);
		sRealRecord[i]->Add(m_RecordPlayBackSpeed[i], 0, wxEXPAND | (wxLEFT), 5);

		sbRealRecord->Add(sRealRecord[i], 0, wxEXPAND | (wxTOP), 2);
	}
	// ==========================================


	// ----------------------------------------------------------------------
	// Set up sizers
	// ----------------
	sbRealBasic->Add(m_ConnectRealWiimote, 0, wxEXPAND | (wxALL), 5);
	sbRealBasic->Add(m_UseRealWiimote, 0, wxEXPAND | (wxALL), 5);

	wxBoxSizer * sRealBasicStatus = new wxBoxSizer(wxHORIZONTAL);
	sRealBasicStatus->Add(sbRealBasic, 0, wxEXPAND | (wxLEFT), 0);
	sRealBasicStatus->Add(sbRealStatus, 0, wxEXPAND | (wxLEFT), 5);

	wxBoxSizer * sRealMain = new wxBoxSizer(wxVERTICAL);
	sRealMain->Add(sRealBasicStatus, 0, wxEXPAND | (wxALL), 5);
	sRealMain->Add(sbRealWiimoteStatus, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
	sRealMain->Add(sbRealRecord, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
	/////////////////////////////////


	////////////////////////////////////////////
	// Set sizers and layout
	// ----------------
	m_PageEmu->SetSizer(sEmulatedMain);
	m_PageReal->SetSizer(sRealMain);
	this->SetSizer(sMain);

	//sEmulatedMain->Layout();
	this->Layout();

	Fit();
	Center();

	ControlsCreated = true;
	/////////////////////////////////
}
/////////////////////////////////


/////////////////////////////////////////////////////////////////////////
/* Record movement */
// ------------

void ConfigDialog::ConvertToString()
{
	// Load ini file
	IniFile file;
	file.Load("WiimoteMovement.ini");
	std::string TmpStr = "", TmpIR = "", TmpTime = "";

	for (int i = 0; i < m_vRecording.size(); i++)
	{
		// Write the movement data
		TmpStr += StringFromFormat("%02x", m_vRecording.at(i).x);
		TmpStr += StringFromFormat("%02x", m_vRecording.at(i).y);
		TmpStr += StringFromFormat("%02x", m_vRecording.at(i).z);
		if(i < (m_vRecording.size() - 1)) TmpStr += ",";

		// Write the IR data
		TmpIR += ArrayToString(m_vRecording.at(i).IR, IRBytes, 0, 30, false);
		if(i < (m_vRecording.size() - 1)) TmpIR += ",";

		// Write the timestamps. The upper limit is 99 seconds.
		int Time = (int)((m_vRecording.at(i).Time - m_vRecording.at(0).Time) * 1000);
		TmpTime += StringFromFormat("%05i", Time);
		if(i < (m_vRecording.size() - 1)) TmpTime += ",";
		//Console::Print("Time: %f %i\n", m_vRecording.at(i).Time, Time);

		/* Break just short of the IniFile.cpp byte limit so that we don't crash file.Load() the next time.
		   This limit should never be hit because of the recording limit below. I keep it here just in case. */
		if(TmpStr.length() > (1024*10 - 10) || TmpIR.length() > (1024*10 - 10) || TmpTime.length() > (1024*10 - 10))
		{
			break;
			PanicAlert("Your recording was to long, the entire recording was not saved.");
		}
	}
	
	// Recordings per second
	double Recordings = (double)m_vRecording.size(); 
	double Time = m_vRecording.at(m_vRecording.size() - 1).Time - m_vRecording.at(0).Time;
	int Rate = (int)(Recordings / Time);

	// If time or the number of recordings are zero we set the Rate to zero
	if (Time == 0 || m_vRecording.size() == 0) Rate = 0;

	// Update GUI	
	m_RecordIRBytesText[m_iRecordTo]->SetValue(wxString::Format(wxT("%i"), IRBytes));
	m_RecordSpeed[m_iRecordTo]->SetValue(wxString::Format(wxT("%i"), Rate));

	// Save file
	std::string SaveName = StringFromFormat("Recording%i", m_iRecordTo);
	file.Set(SaveName.c_str(), "Movement", TmpStr.c_str());
	file.Set(SaveName.c_str(), "IR", TmpIR.c_str());
	file.Set(SaveName.c_str(), "Time", TmpTime.c_str());
	file.Set(SaveName.c_str(), "IRBytes", IRBytes);
	file.Set(SaveName.c_str(), "RecordingSpeed", Rate);

	// Set a default playback speed if none is set already
	int TmpPlaySpeed; file.Get(SaveName.c_str(), "PlaybackSpeed", &TmpPlaySpeed, -1);
	if (TmpPlaySpeed == -1)
	{
		file.Set(SaveName.c_str(), "PlaybackSpeed", 3);
		m_RecordPlayBackSpeed[m_iRecordTo]->SetSelection(3);
	}	

	file.Save("WiimoteMovement.ini");
}

// Timeout the recording
void ConfigDialog::Update(wxTimerEvent& WXUNUSED(event))
{
	m_bWaitForRecording = false;
	m_bRecording = false;
	m_RecordButton[m_iRecordTo]->SetLabel(wxT(""));
	UpdateGUI();
}

// One second timeout for another A press
void ConfigDialog::UpdateA(wxTimerEvent& WXUNUSED(event))
{
	m_bAllowA = true;
	Console::Print("A allowed again");
}

void ConfigDialog::RecordMovement(wxCommandEvent& event)
{
	m_iRecordTo = event.GetId() - 2000;

	if(WiiMoteReal::g_MotionSensing)
	{
		// Check if there already is a recording here
		if(m_RecordSpeed[m_iRecordTo]->GetLineLength(0) > 0)
		{
			if(!AskYesNo("Do you want to replace the current recording?")) return;
		}
		m_RecordButton[m_iRecordTo]->SetLabel(wxT("Hold A"));
	}
	else
	{
		m_RecordButton[m_iRecordTo]->SetLabel(wxT("Press +"));
		return;
	}

	m_bWaitForRecording = true;
	m_bAllowA = true;
	m_bRecording = false;

	UpdateGUI();

	m_TimeoutTimer->Start(5000, true);
	//m_TimeoutATimer->Start(500, true);
}

void ConfigDialog::DoRecordA(bool Pressed)
{
	// Return if we are not waiting or recording
	if (! (m_bWaitForRecording || m_bRecording)) return;

	// Return if we are waiting but have not pressed A
	if (m_bWaitForRecording && !Pressed) return;

	// Return if we are recording but are still pressing A
	if (m_bRecording && Pressed) return;

	//m_bAllowA = false;
	m_bRecording = Pressed;

	// Start recording, only run this once
	if(m_bRecording && m_bWaitForRecording)
	{
		m_RecordButton[m_iRecordTo]->SetLabel(wxT("Recording..."));
		m_vRecording.clear(); // Clear the list
		m_TimeoutTimer->Stop();
		m_bWaitForRecording = false;
	}
	// The recording is done
	else
	{
		m_RecordButton[m_iRecordTo]->SetLabel(wxT("Done"));
		Console::Print("Done: %i %i\n", m_bWaitForRecording, m_bRecording);
		//m_bAllowA = true;
		ConvertToString();
	}

	UpdateGUI();
}

void ConfigDialog::DoRecordMovement(u8 _x, u8 _y, u8 _z, const u8 *_IR, int _IRBytes)
{
	//std::string Tmp1 = ArrayToString(_IR, 20, 0, 30);
	//Console::Print("DoRecordMovement: %s\n", Tmp1.c_str());

	if (!m_bRecording) return;

	//Console::Print("DoRecordMovement\n");

	SRecording Tmp;
	Tmp.x = _x;
	Tmp.y = _y;
	Tmp.z = _z;
	Tmp.Time = GetDoubleTime();
	memcpy(Tmp.IR, _IR, _IRBytes);
	m_vRecording.push_back(Tmp);

	// Save the number of IR bytes
	IRBytes = _IRBytes;

	/* The upper limit of a recording coincides with the IniFile.cpp limit, each list element
	   is 7 bytes, therefore be divide by 7 */
	if (m_vRecording.size() > (10*1024 / 7 - 2) )
	{
		m_bRecording = false;
		m_RecordButton[m_iRecordTo]->SetLabel(wxT("Done"));
		ConvertToString();
		UpdateGUI();
	}
}
/////////////////////////////////


// ===================================================
/* Do use real wiimote */
// ----------------
void ConfigDialog::DoConnectReal()
{
	g_Config.bConnectRealWiimote = m_ConnectRealWiimote->IsChecked();

	if(g_Config.bConnectRealWiimote)
	{
		if (!g_RealWiiMoteInitialized) WiiMoteReal::Initialize();
	}
	else
	{
		if (g_RealWiiMoteInitialized) WiiMoteReal::Shutdown();
	}
}


// ===================================================
/* Generate connect/disconnect status event */
// ----------------
void ConfigDialog::DoExtensionConnectedDisconnected()
{
	// There is no need for this if no game is running
	if(!g_EmulatorRunning) return; 

	u8 DataFrame[8]; // make a blank report for it
	wm_request_status *rs = (wm_request_status*)DataFrame;

	// Check if a game is running, in that case change the status
	if(WiiMoteEmu::g_ReportingChannel > 0)
		WiiMoteEmu::WmRequestStatus(WiiMoteEmu::g_ReportingChannel, rs);
}


// ===================================================
/* Change settings */
// ----------------
void ConfigDialog::GeneralSettingsChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_SIDEWAYSDPAD:
		g_Config.bSidewaysDPad = m_SidewaysDPad->IsChecked();
		break;

	case ID_WIDESCREEN:
		g_Config.bWideScreen = m_WideScreen->IsChecked();
		break;

	case ID_NUNCHUCKCONNECTED:
		// Don't allow two extensions at the same time
		if(m_ClassicControllerConnected->IsChecked())
		{
			m_ClassicControllerConnected->SetValue(false);
			g_Config.bClassicControllerConnected = false;
			// Disconnect the extension so that the game recognize the change
			DoExtensionConnectedDisconnected();
			/* It doesn't seem to be needed but shouldn't it at least take 25 ms to
			   reconnect an extension after we disconnected another? */
			if(g_EmulatorRunning) Sleep(25);
		}

		// Update status
		g_Config.bNunchuckConnected = m_NunchuckConnected->IsChecked();

		// Copy the calibration data
		memcpy(WiiMoteEmu::g_RegExt + 0x20, WiiMoteEmu::nunchuck_calibration,
			sizeof(WiiMoteEmu::nunchuck_calibration));
		memcpy(WiiMoteEmu::g_RegExt + 0xfa, WiiMoteEmu::nunchuck_id, sizeof(WiiMoteEmu::nunchuck_id));

		// Generate connect/disconnect status event
		DoExtensionConnectedDisconnected();
		break;

	case ID_CLASSICCONTROLLERCONNECTED:
		// Don't allow two extensions at the same time
		if(m_NunchuckConnected->IsChecked())
		{
			m_NunchuckConnected->SetValue(false);
			g_Config.bNunchuckConnected = false;
			// Disconnect the extension so that the game recognize the change
			DoExtensionConnectedDisconnected();
		}

		g_Config.bClassicControllerConnected = m_ClassicControllerConnected->IsChecked();

		// Copy the calibration data
		memcpy(WiiMoteEmu::g_RegExt + 0x20, WiiMoteEmu::classic_calibration,
			sizeof(WiiMoteEmu::classic_calibration));
		memcpy(WiiMoteEmu::g_RegExt + 0xfa, WiiMoteEmu::classic_id, sizeof(WiiMoteEmu::classic_id));
		// Generate connect/disconnect status event
		DoExtensionConnectedDisconnected();
		break;


		//////////////////////////
		// Real Wiimote
		// -----------
	case ID_CONNECT_REAL:
		DoConnectReal();
		break;
	case ID_USE_REAL:
		g_Config.bUseRealWiimote = m_UseRealWiimote->IsChecked();
		//if(g_Config.bUseRealWiimote) WiiMoteReal::SetDataReportingMode();
		if(g_Config.bUseRealWiimote) WiiMoteReal::ClearEvents();		
		break;
	case ID_UPDATE_REAL:
		g_Config.bUpdateRealWiimote = m_UpdateMeters->IsChecked();
		break;

	case IDC_RECORD + 1:
	case IDC_RECORD + 2:
	case IDC_RECORD + 3:
	case IDC_RECORD + 4:
	case IDC_RECORD + 5:
	case IDC_RECORD + 6:
	case IDC_RECORD + 7:
	case IDC_RECORD + 8:
	case IDC_RECORD + 9:
	case IDC_RECORD + 10:
	case IDC_RECORD + 11:
	case IDC_RECORD + 12:
	case IDC_RECORD + 13:
	case IDC_RECORD + 14:
	case IDC_RECORD + 15:
		// Check if any of the other choice boxes has the same hotkey
		for (int i = 1; i < (RECORDING_ROWS + 1); i++)
		{
			int CurrentChoiceBox = (event.GetId() - IDC_RECORD);
			if (i == CurrentChoiceBox) continue;
			if (m_RecordHotKey[i]->GetSelection() == m_RecordHotKey[CurrentChoiceBox]->GetSelection()) m_RecordHotKey[i]->SetSelection(10);
			Console::Print("HotKey: %i %i\n",
				m_RecordHotKey[i]->GetSelection(), m_RecordHotKey[CurrentChoiceBox]->GetSelection());
		}
		break;
		/////////////////
	}
	g_Config.Save();
	UpdateGUI();
}



// =======================================================
// Update the enabled/disabled status
// -------------
void ConfigDialog::UpdateGUI()
{
	Console::Print("UpdateGUI: \n");

	/* We can't allow different values for this one if we are using the real and emulated wiimote
	   side by side so that we can switch between between during gameplay. We update the checked
	   or unchecked values from the g_Config settings, and we make sure they are up to date with
	   unplugged and reinserted extensions. */	
	m_NunchuckConnected->SetValue(g_Config.bNunchuckConnected);
	m_ClassicControllerConnected->SetValue(g_Config.bClassicControllerConnected);
	m_NunchuckConnected->Enable(!(g_RealWiiMotePresent && g_Config.bConnectRealWiimote && g_EmulatorRunning));
	m_ClassicControllerConnected->Enable(!(g_RealWiiMotePresent && g_Config.bConnectRealWiimote && g_EmulatorRunning));

	/* I have disabled this option during a running game because it's enough to be able to switch
	   between using and not using then. To also use the connect option during a running game would
	   mean that the wiimote must be sent the current reporting mode and the channel ID after it
	   has been initialized. Functions for that are basically already in place so these two options
	   could possibly be simplified to one option. */
	m_ConnectRealWiimote->Enable(!g_EmulatorRunning);
	m_UseRealWiimote->Enable(g_RealWiiMotePresent && g_Config.bConnectRealWiimote);

// Linux has no FindItem()
#ifdef _WIN32
	for(int i = IDB_RECORD + 1; i < (IDB_RECORD + RECORDING_ROWS + 1); i++)
		if(ControlsCreated) m_Notebook->FindItem(i)->Enable(!(m_bWaitForRecording || m_bRecording));
#endif
}
