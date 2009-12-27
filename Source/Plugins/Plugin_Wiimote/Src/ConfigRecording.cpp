// Copyright (C) 2003 Dolphin Project.

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

#include "IniFile.h"
#include "Timer.h"

#include "wiimote_real.h" // Local
#include "wiimote_hid.h"
#include "main.h"
#include "ConfigRecordingDlg.h"
#include "ConfigBasicDlg.h"
#include "Config.h"
#include "EmuMain.h" // for LoadRecordedMovements()
#include "EmuSubroutines.h" // for WmRequestStatus

void WiimoteRecordingConfigDialog::LoadFile()
{
	DEBUG_LOG(WIIMOTE, "LoadFile()");

	IniFile file;
	file.Load(FULL_CONFIG_DIR "WiimoteMovement.ini");

	for (int i = 1; i < (RECORDING_ROWS + 1); i++)
	{
		// Temporary storage
		int iTmp;
		std::string STmp;

		// Get row name
		std::string SaveName = StringFromFormat("Recording%i", i);

		// HotKey
		file.Get(SaveName.c_str(), "HotKeySwitch", &iTmp, 3); m_RecordHotKeySwitch[i]->SetSelection(iTmp);
		file.Get(SaveName.c_str(), "HotKeyWiimote", &iTmp, 10); m_RecordHotKeyWiimote[i]->SetSelection(iTmp);		
		file.Get(SaveName.c_str(), "HotKeyNunchuck", &iTmp, 10); m_RecordHotKeyNunchuck[i]->SetSelection(iTmp);
		file.Get(SaveName.c_str(), "HotKeyIR", &iTmp, 10); m_RecordHotKeyIR[i]->SetSelection(iTmp);

		// Movement name
		file.Get(SaveName.c_str(), "MovementName", &STmp, ""); m_RecordText[i]->SetValue(wxString::FromAscii(STmp.c_str()));

		// Game name
		file.Get(SaveName.c_str(), "GameName", &STmp, ""); m_RecordGameText[i]->SetValue(wxString::FromAscii(STmp.c_str()));

		// IR Bytes
		file.Get(SaveName.c_str(), "IRBytes", &STmp, ""); m_RecordIRBytesText[i]->SetValue(wxString::FromAscii(STmp.c_str()));

		// Recording speed
		file.Get(SaveName.c_str(), "RecordingSpeed", &iTmp, -1);
		if(iTmp != -1)
			m_RecordSpeed[i]->SetValue(wxString::Format(wxT("%i"), iTmp));
		else
			m_RecordSpeed[i]->SetValue(wxT(""));

		// Playback speed (currently always saved directly after a recording)
		file.Get(SaveName.c_str(), "PlaybackSpeed", &iTmp, -1); m_RecordPlayBackSpeed[i]->SetSelection(iTmp);
	}
}

void WiimoteRecordingConfigDialog::SaveFile()
{
	DEBUG_LOG(WIIMOTE, "SaveFile");

	IniFile file;
	file.Load(FULL_CONFIG_DIR "WiimoteMovement.ini");

	for(int i = 1; i < (RECORDING_ROWS + 1); i++)
	{
		// Get row name
		std::string SaveName = StringFromFormat("Recording%i", i);

		// HotKey
		file.Set(SaveName.c_str(), "HotKeySwitch", m_RecordHotKeySwitch[i]->GetSelection());
		file.Set(SaveName.c_str(), "HotKeyWiimote", m_RecordHotKeyWiimote[i]->GetSelection());
		file.Set(SaveName.c_str(), "HotKeyNunchuck", m_RecordHotKeyNunchuck[i]->GetSelection());
		file.Set(SaveName.c_str(), "HotKeyIR", m_RecordHotKeyIR[i]->GetSelection());

		// Movement name
		file.Set(SaveName.c_str(), "MovementName", (const char*)m_RecordText[i]->GetValue().mb_str(wxConvUTF8));

		// Game name
		file.Set(SaveName.c_str(), "GameName", (const char*)m_RecordGameText[i]->GetValue().mb_str(wxConvUTF8));

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

	file.Save(FULL_CONFIG_DIR "WiimoteMovement.ini");
	DEBUG_LOG(WIIMOTE, "SaveFile()");
}

void WiimoteRecordingConfigDialog::CreateGUIControlsRecording()
{
	m_PageRecording = new wxPanel(this, ID_RECORDINGPAGE, wxDefaultPosition, wxDefaultSize);

	m_TextUpdateRate = new wxStaticText(m_PageRecording, wxID_ANY, wxT("Update rate: 000 times/s"));
	m_UpdateMeters = new wxCheckBox(m_PageRecording, ID_UPDATE_REAL, wxT("Update gauges"));

	m_UpdateMeters->SetValue(g_Config.bUpdateRealWiimote);

	m_UpdateMeters->SetToolTip(
		wxT("You can turn this off when a game is running to avoid a potential slowdown that may come from redrawing the\n")
		wxT("configuration screen. Remember that you also need to press '+' on your Wiimote before you can record movements."));

	// Width and height of the gauges
	static const int Gw = 35, Gh = 110; //ugly

	m_GaugeBattery = new wxGauge( m_PageRecording, wxID_ANY, 100, wxDefaultPosition, wxSize(Gw, Gh), wxGA_VERTICAL | wxNO_BORDER | wxGA_SMOOTH);
	m_GaugeRoll[0] = new wxGauge( m_PageRecording, wxID_ANY, 360, wxDefaultPosition, wxSize(Gw, Gh), wxGA_VERTICAL | wxNO_BORDER | wxGA_SMOOTH);
	m_GaugeRoll[1] = new wxGauge( m_PageRecording, wxID_ANY, 360, wxDefaultPosition, wxSize(Gw, Gh), wxGA_VERTICAL | wxNO_BORDER | wxGA_SMOOTH);
	m_GaugeGForce[0] = new wxGauge( m_PageRecording, wxID_ANY, 600, wxDefaultPosition, wxSize(Gw, Gh), wxGA_VERTICAL | wxNO_BORDER | wxGA_SMOOTH);
	m_GaugeGForce[1] = new wxGauge( m_PageRecording, wxID_ANY, 600, wxDefaultPosition, wxSize(Gw, Gh), wxGA_VERTICAL | wxNO_BORDER | wxGA_SMOOTH);
	m_GaugeGForce[2] = new wxGauge( m_PageRecording, wxID_ANY, 600, wxDefaultPosition, wxSize(Gw, Gh), wxGA_VERTICAL | wxNO_BORDER | wxGA_SMOOTH);
	m_GaugeAccel[0] = new wxGauge( m_PageRecording, wxID_ANY, 255, wxDefaultPosition, wxSize(Gw, Gh), wxGA_VERTICAL | wxNO_BORDER | wxGA_SMOOTH);
	m_GaugeAccel[1] = new wxGauge( m_PageRecording, wxID_ANY, 255, wxDefaultPosition, wxSize(Gw, Gh), wxGA_VERTICAL | wxNO_BORDER | wxGA_SMOOTH);
	m_GaugeAccel[2] = new wxGauge( m_PageRecording, wxID_ANY, 255, wxDefaultPosition, wxSize(Gw, Gh), wxGA_VERTICAL | wxNO_BORDER | wxGA_SMOOTH);

	// The text controls
	m_TextIR = new wxStaticText(m_PageRecording, wxID_ANY, wxT("Cursor: 000 000\nDistance: 0000"));

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

	wxStaticText * m_TextBattery = new wxStaticText(m_PageRecording, wxID_ANY, wxT("Batt."), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
	wxStaticText * m_TextRoll = new wxStaticText(m_PageRecording, wxID_ANY, wxT("Roll"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
	wxStaticText * m_TextPitch = new wxStaticText(m_PageRecording, wxID_ANY, wxT("Pitch"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
	wxStaticText *m_TextX[2], *m_TextY[2], *m_TextZ[2];
	m_TextX[0] = new wxStaticText(m_PageRecording, wxID_ANY, wxT("X"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE); m_TextX[1] = new wxStaticText(m_PageRecording, wxID_ANY, wxT("X"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
	m_TextY[0] = new wxStaticText(m_PageRecording, wxID_ANY, wxT("Y"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE); m_TextY[1] = new wxStaticText(m_PageRecording, wxID_ANY, wxT("Y"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
	m_TextZ[0] = new wxStaticText(m_PageRecording, wxID_ANY, wxT("Z"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE); m_TextZ[1] = new wxStaticText(m_PageRecording, wxID_ANY, wxT("Z"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);

	sBoxBattery->Add(m_GaugeBattery, 0, wxEXPAND | (wxALL), 0); sBoxBattery->Add(m_TextBattery, 0, wxEXPAND | (wxUP), 5);

	sBoxRoll[0]->Add(m_GaugeRoll[0], 0, wxEXPAND | (wxUP | wxDOWN | wxLEFT), 0); sBoxRoll[0]->Add(m_TextRoll, 0, wxEXPAND | (wxUP), 5);
	sBoxRoll[1]->Add(m_GaugeRoll[1], 0, wxEXPAND | (wxUP | wxDOWN | wxRIGHT), 0); sBoxRoll[1]->Add(m_TextPitch, 0, wxEXPAND | (wxUP), 5);

	sBoxGForce[0]->Add(m_GaugeGForce[0], 0, wxEXPAND | (wxUP | wxDOWN | wxLEFT), 0); sBoxGForce[0]->Add(m_TextX[0], 0, wxEXPAND | (wxUP), 5);
	sBoxGForce[1]->Add(m_GaugeGForce[1], 0, wxEXPAND | (wxUP | wxDOWN), 0); sBoxGForce[1]->Add(m_TextY[0], 0, wxEXPAND | (wxUP), 5);
	sBoxGForce[2]->Add(m_GaugeGForce[2], 0, wxEXPAND | (wxUP | wxDOWN | wxRIGHT), 0); sBoxGForce[2]->Add(m_TextZ[0], 0, wxEXPAND | (wxUP), 5);

	sBoxAccel[0]->Add(m_GaugeAccel[0], 0, wxEXPAND | (wxUP | wxDOWN | wxLEFT), 0); sBoxAccel[0]->Add(m_TextX[1], 0, wxEXPAND | (wxUP), 5);
	sBoxAccel[1]->Add(m_GaugeAccel[1], 0, wxEXPAND | (wxUP | wxDOWN), 0); sBoxAccel[1]->Add(m_TextY[1], 0, wxEXPAND | (wxUP), 5);
	sBoxAccel[2]->Add(m_GaugeAccel[2], 0, wxEXPAND | (wxUP | wxDOWN | wxRIGHT), 0); sBoxAccel[2]->Add(m_TextZ[1], 0, wxEXPAND | (wxUP), 5);

	wxStaticBoxSizer * sbRealStatus = new wxStaticBoxSizer(wxVERTICAL, m_PageRecording, wxT("Status"));
	wxStaticBoxSizer * sbRealIR = new wxStaticBoxSizer(wxHORIZONTAL, m_PageRecording, wxT("IR"));
	wxStaticBoxSizer * sbRealBattery = new wxStaticBoxSizer(wxVERTICAL, m_PageRecording, wxT("Battery"));
	wxStaticBoxSizer * sbRealRoll = new wxStaticBoxSizer(wxHORIZONTAL, m_PageRecording, wxT("Roll and Pitch"));
	wxStaticBoxSizer * sbRealGForce = new wxStaticBoxSizer(wxHORIZONTAL, m_PageRecording, wxT("G-Force"));
	wxStaticBoxSizer * sbRealAccel = new wxStaticBoxSizer(wxHORIZONTAL, m_PageRecording, wxT("Accelerometer"));

	// Status
	sbRealStatus->Add(m_TextUpdateRate, 0, wxEXPAND | (wxALL), 5);
	sbRealStatus->Add(m_UpdateMeters, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);

	sbRealIR->Add(m_TextIR, 0, wxEXPAND | (wxALL), 5);
	sbRealBattery->Add(sBoxBattery, 0, wxEXPAND | (wxALL), 5);
	sbRealRoll->Add(sBoxRoll[0], 0, wxEXPAND | (wxALL), 5); sbRealRoll->Add(sBoxRoll[1], 0, wxEXPAND | (wxALL), 5);
	sbRealGForce->Add(sBoxGForce[0], 0, wxEXPAND | (wxALL), 5); sbRealGForce->Add(sBoxGForce[1], 0, wxEXPAND | (wxALL), 5); sbRealGForce->Add(sBoxGForce[2], 0, wxEXPAND | (wxALL), 5);
	sbRealAccel->Add(sBoxAccel[0], 0, wxEXPAND | (wxALL), 5); sbRealAccel->Add(sBoxAccel[1], 0, wxEXPAND | (wxALL), 5); sbRealAccel->Add(sBoxAccel[2], 0, wxEXPAND | (wxALL), 5);

	// Vertical leftmost status
	wxBoxSizer * sbStatusLeft = new wxBoxSizer(wxVERTICAL);
	sbStatusLeft->Add(sbRealStatus, 0, wxEXPAND | (wxLEFT), 0);
	sbStatusLeft->Add(sbRealIR, 0, wxEXPAND | (wxLEFT), 0);

	wxBoxSizer * sbRealWiimoteStatus = new wxBoxSizer(wxHORIZONTAL);
	sbRealWiimoteStatus->Add(sbStatusLeft, 0, wxEXPAND | (wxLEFT), 0);
	sbRealWiimoteStatus->Add(sbRealBattery, 0, wxEXPAND | (wxLEFT), 5);
	sbRealWiimoteStatus->Add(sbRealRoll, 0, wxEXPAND | (wxLEFT), 5);
	sbRealWiimoteStatus->Add(sbRealGForce, 0, wxEXPAND | (wxLEFT), 5);
	sbRealWiimoteStatus->Add(sbRealAccel, 0, wxEXPAND | (wxLEFT), 5);

	m_GaugeBattery->SetToolTip(wxT("Press '+' to show the current status. Press '-' to stop recording the status."));

	wxStaticBoxSizer * sbRealRecord = new wxStaticBoxSizer(wxVERTICAL, m_PageRecording, wxT("Record movements"));

	wxArrayString StrHotKeySwitch;
	StrHotKeySwitch.Add(wxT("Shift"));
	StrHotKeySwitch.Add(wxT("Ctrl"));
	StrHotKeySwitch.Add(wxT("Alt"));
	StrHotKeySwitch.Add(wxT(""));

	wxArrayString StrHotKey;
	for(int i = 0; i < 10; i++) StrHotKey.Add(wxString::Format(wxT("%i"), i));
	StrHotKey.Add(wxT(""));

	wxArrayString StrPlayBackSpeed;
	for(int i = 1; i <= 20; i++) StrPlayBackSpeed.Add(wxString::Format(wxT("%i"), i*25));

	wxBoxSizer * sRealRecord[RECORDING_ROWS + 1];

	wxStaticText * m_TextRec = new wxStaticText(m_PageRecording, wxID_ANY, wxT("Rec."), wxDefaultPosition, wxSize(80, -1), wxALIGN_CENTRE);
	wxStaticText * m_TextHotKeyWm = new wxStaticText(m_PageRecording, wxID_ANY, wxT("Wiim."), wxDefaultPosition, wxSize(32, -1), wxALIGN_CENTRE);
	wxStaticText * m_TextHotKeyNc = new wxStaticText(m_PageRecording, wxID_ANY, wxT("Nunc."), wxDefaultPosition, wxSize(32, -1), wxALIGN_CENTRE);
	wxStaticText * m_TextHotKeyIR = new wxStaticText(m_PageRecording, wxID_ANY, wxT("IR"), wxDefaultPosition, wxSize(32, -1), wxALIGN_CENTRE);
	wxStaticText * m_TextMovement = new wxStaticText(m_PageRecording, wxID_ANY, wxT("Movement name"), wxDefaultPosition, wxSize(200, -1), wxALIGN_CENTRE);
	wxStaticText * m_TextGame = new wxStaticText(m_PageRecording, wxID_ANY, wxT("Game name"), wxDefaultPosition, wxSize(200, -1), wxALIGN_CENTRE);
	wxStaticText * m_TextIRBytes = new wxStaticText(m_PageRecording, wxID_ANY, wxT("IR"), wxDefaultPosition, wxSize(20, -1), wxALIGN_CENTRE);
	wxStaticText * m_TextRecSpeed = new wxStaticText(m_PageRecording, wxID_ANY, wxT("R. s."), wxDefaultPosition, wxSize(33, -1), wxALIGN_CENTRE);
	wxStaticText * m_TextPlaySpeed = new wxStaticText(m_PageRecording, wxID_ANY, wxT("Pl. s."), wxDefaultPosition, wxSize(40, -1), wxALIGN_CENTRE);
	
	// Tool tips
	m_TextRec->SetToolTip(
		wxT("To record a movement first press this button, then start the recording by pressing 'A' on the Wiimote and stop the recording\n")
		wxT("by letting go of 'A'"));
	m_TextHotKeyWm->SetToolTip(
		wxT("Select a hotkey for playback of Wiimote movements. You can combine it with an")
		wxT(" optional Shift, Ctrl, or Alt switch."));
	m_TextHotKeyNc->SetToolTip(wxT(
		"Select a hotkey for playback of Nunchuck movements"));
	m_TextHotKeyIR->SetToolTip(wxT(
		"Select a hotkey for playback of Nunchuck movements"));
	m_TextRecSpeed->SetToolTip(wxT(
		"Recording speed in average measurements per second"));
	m_TextPlaySpeed->SetToolTip(
		wxT("Playback speed: A playback speed of 100 means that the playback occurs at the same rate as it was recorded. (You can see the\n")
		wxT("current update rate in the Status window above when a game is running.) However, if your framerate is only at 50% of full speed\n")
		wxT("you may want to select a playback rate of 50, because then the game might perceive the playback as a full speed playback. (This\n")
		wxT("holds if Wiimote_Update() is tied to the framerate, I'm not sure that this is the case. It seemed to vary somewhat with different\n")
		wxT("framerates but perhaps not enough to say that it was exactly tied to the framerate.) So until this is better understood you'll have\n")
		wxT("to try different playback rates and see which one that works."));

	sRealRecord[0] = new wxBoxSizer(wxHORIZONTAL);
	sRealRecord[0]->Add(m_TextRec, 0, wxEXPAND | (wxLEFT), 5);
	sRealRecord[0]->Add(m_TextHotKeyWm, 0, wxEXPAND | (wxLEFT), 62);
	sRealRecord[0]->Add(m_TextHotKeyNc, 0, wxEXPAND | (wxLEFT), 5);
	sRealRecord[0]->Add(m_TextHotKeyIR, 0, wxEXPAND | (wxLEFT), 5);
	sRealRecord[0]->Add(m_TextMovement, 0, wxEXPAND | (wxLEFT), 5);
	sRealRecord[0]->Add(m_TextGame, 0, wxEXPAND | (wxLEFT), 5);
	sRealRecord[0]->Add(m_TextIRBytes, 0, wxEXPAND | (wxLEFT), 5);
	sRealRecord[0]->Add(m_TextRecSpeed, 0, wxEXPAND | (wxLEFT), 5);
	sRealRecord[0]->Add(m_TextPlaySpeed, 0, wxEXPAND | (wxLEFT), 5);
	sbRealRecord->Add(sRealRecord[0], 0, wxEXPAND | (wxALL), 0);

	for(int i = 1; i < (RECORDING_ROWS + 1); i++)
	{
		sRealRecord[i] = new wxBoxSizer(wxHORIZONTAL);
		m_RecordButton[i] = new wxButton(m_PageRecording, IDB_RECORD + i, wxEmptyString, wxDefaultPosition, wxSize(80, 20), 0, wxDefaultValidator, wxEmptyString);
		m_RecordHotKeySwitch[i] = new wxChoice(m_PageRecording, IDC_RECORD + i, wxDefaultPosition, wxDefaultSize, StrHotKeySwitch);
		m_RecordHotKeyWiimote[i] = new wxChoice(m_PageRecording, IDC_RECORD + i, wxDefaultPosition, wxDefaultSize, StrHotKey);
		m_RecordHotKeyNunchuck[i] = new wxChoice(m_PageRecording, IDC_RECORD + i, wxDefaultPosition, wxDefaultSize, StrHotKey);
		m_RecordHotKeyIR[i] = new wxChoice(m_PageRecording, IDC_RECORD + i, wxDefaultPosition, wxDefaultSize, StrHotKey);
		m_RecordText[i] = new wxTextCtrl(m_PageRecording, IDT_RECORD_TEXT, wxT(""), wxDefaultPosition, wxSize(200, 19));
		m_RecordGameText[i] = new wxTextCtrl(m_PageRecording, IDT_RECORD_GAMETEXT, wxT(""), wxDefaultPosition, wxSize(200, 19));
		m_RecordIRBytesText[i] = new wxTextCtrl(m_PageRecording, IDT_RECORD_IRBYTESTEXT, wxT(""), wxDefaultPosition, wxSize(25, 19));
		m_RecordSpeed[i] = new wxTextCtrl(m_PageRecording, IDT_RECORD_SPEED, wxT(""), wxDefaultPosition, wxSize(30, 19), wxTE_READONLY | wxTE_CENTRE);
		m_RecordPlayBackSpeed[i] = new wxChoice(m_PageRecording, IDT_RECORD_PLAYSPEED, wxDefaultPosition, wxDefaultSize, StrPlayBackSpeed);

		m_RecordText[i]->SetMaxLength(35);
		m_RecordGameText[i]->SetMaxLength(35);
		m_RecordIRBytesText[i]->Enable(false);
		m_RecordSpeed[i]->Enable(false);

		// Row 2 Sizers
		sRealRecord[i]->Add(m_RecordButton[i], 0, wxEXPAND | (wxLEFT), 5);
		sRealRecord[i]->Add(m_RecordHotKeySwitch[i], 0, wxEXPAND | (wxLEFT), 5);
		sRealRecord[i]->Add(m_RecordHotKeyWiimote[i], 0, wxEXPAND | (wxLEFT), 2);
		sRealRecord[i]->Add(m_RecordHotKeyNunchuck[i], 0, wxEXPAND | (wxLEFT), 2);
		sRealRecord[i]->Add(m_RecordHotKeyIR[i], 0, wxEXPAND | (wxLEFT), 2);
		sRealRecord[i]->Add(m_RecordText[i], 0, wxEXPAND | (wxLEFT), 5);
		sRealRecord[i]->Add(m_RecordGameText[i], 0, wxEXPAND | (wxLEFT), 5);
		sRealRecord[i]->Add(m_RecordIRBytesText[i], 0, wxEXPAND | (wxLEFT), 5);
		sRealRecord[i]->Add(m_RecordSpeed[i], 0, wxEXPAND | (wxLEFT), 5);
		sRealRecord[i]->Add(m_RecordPlayBackSpeed[i], 0, wxEXPAND | (wxLEFT), 5);

		sbRealRecord->Add(sRealRecord[i], 0, wxEXPAND | (wxTOP), 2);
	}

	m_sRecordingMain = new wxBoxSizer(wxVERTICAL);
	m_sRecordingMain->Add(sbRealWiimoteStatus, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxUP), 5);
	m_sRecordingMain->Add(sbRealRecord, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);

	m_PageRecording->SetSizer(m_sRecordingMain);

	m_Apply = new wxButton(this, ID_APPLY, wxT("Apply"));
	m_Close = new wxButton(this, ID_CLOSE, wxT("Close"));
	m_Close->SetToolTip(wxT("Apply and Close"));

	wxBoxSizer* sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->AddStretchSpacer();
	sButtons->Add(m_Apply, 0, (wxALL), 0);
	sButtons->Add(m_Close, 0, (wxLEFT), 5);	

	m_MainSizer = new wxBoxSizer(wxVERTICAL);
	m_MainSizer->Add(m_PageRecording, 1, wxEXPAND | wxALL, 5);
	m_MainSizer->Add(sButtons, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);

	this->SetSizer(m_MainSizer);
	this->Layout();

	Fit();

	// Center the window if there is room for it
	#ifdef _WIN32
		if (GetSystemMetrics(SM_CYFULLSCREEN) > 800)
			Center();
	#endif

	ControlsCreated = true;
}

void WiimoteRecordingConfigDialog::ConvertToString()
{
	IniFile file;
	file.Load(FULL_CONFIG_DIR "WiimoteMovement.ini");
	std::string TmpStr = "", TmpIR = "", TmpTime = "";

	for (int i = 0; i < (int)m_vRecording.size(); i++)
	{
		// Write the movement data
		TmpStr += StringFromFormat("%s", m_vRecording.at(i).x >= 0 ? StringFromFormat("+%03i", m_vRecording.at(i).x).c_str() : StringFromFormat("%04i", m_vRecording.at(i).x).c_str());
		TmpStr += StringFromFormat("%s", m_vRecording.at(i).y >= 0 ? StringFromFormat("+%03i", m_vRecording.at(i).y).c_str() : StringFromFormat("%04i", m_vRecording.at(i).y).c_str());
		TmpStr += StringFromFormat("%s", m_vRecording.at(i).z >= 0 ? StringFromFormat("+%03i", m_vRecording.at(i).z).c_str() : StringFromFormat("%04i", m_vRecording.at(i).z).c_str());
		if (i < ((int)m_vRecording.size() - 1)) TmpStr += ",";

		//DEBUG_LOG(WIIMOTE, "%s", TmpStr.c_str());

		// Write the IR data
		TmpIR += ArrayToString(m_vRecording.at(i).IR, IRBytes, 0, 30, false);
		if (i < ((int)m_vRecording.size() - 1)) TmpIR += ",";

		// Write the timestamps. The upper limit is 99 seconds.
		int Time = (int)((m_vRecording.at(i).Time - m_vRecording.at(0).Time) * 1000);
		TmpTime += StringFromFormat("%05i", Time);
		if (i < ((int)m_vRecording.size() - 1)) TmpTime += ",";

		/* Break just short of the IniFile.cpp byte limit so that we don't
		   crash file.Load() the next time.  This limit should never be hit
		   because of the recording limit below. I keep it here just in
		   case. */
		if(TmpStr.length() > (1024*10 - 10) || TmpIR.length() > (1024*10 - 10) || TmpTime.length() > (1024*10 - 10))
		{
			break;
			PanicAlert("Your recording was to long, the entire recording was not saved.");
		}

		// Debug
		DEBUG_LOG(WIIMOTE, "Saved: [%i / %i] %03i %03i %03i", i, m_vRecording.size(), m_vRecording.at(i).x, m_vRecording.at(i).y, m_vRecording.at(i).z);
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

	file.Save(FULL_CONFIG_DIR "WiimoteMovement.ini");

	DEBUG_LOG(WIIMOTE, "Save recording to WiimoteMovement.ini");
}

// Timeout the recording
void WiimoteRecordingConfigDialog::Update(wxTimerEvent& WXUNUSED(event))
{
	m_bWaitForRecording = false;
	m_bRecording = false;
	m_RecordButton[m_iRecordTo]->SetLabel(wxT(""));
	UpdateRecordingGUI();
}

void WiimoteRecordingConfigDialog::RecordMovement(wxCommandEvent& event)
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
	m_bRecording = false;

	UpdateRecordingGUI();

	m_TimeoutTimer->Start(5000, true);
}

void WiimoteRecordingConfigDialog::DoRecordA(bool Pressed)
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
		DEBUG_LOG(WIIMOTE, "Done: %i %i", m_bWaitForRecording, m_bRecording);
		//m_bAllowA = true;
		ConvertToString();
	}

	UpdateRecordingGUI();
}

void WiimoteRecordingConfigDialog::DoRecordMovement(int _x, int _y, int _z, const u8 *_IR, int _IRBytes)
{
	//std::string Tmp1 = ArrayToString(_IR, 20, 0, 30);
	//DEBUG_LOG(WIIMOTE, "DoRecordMovement: %s", Tmp1.c_str());

	if (!m_bRecording) return;

	//DEBUG_LOG(WIIMOTE, "DoRecordMovement: %03i %03i %03i", _x, _y, _z);

	SRecording Tmp;
	Tmp.x = _x;
	Tmp.y = _y;
	Tmp.z = _z;
	Tmp.Time = Common::Timer::GetDoubleTime();
	memcpy(Tmp.IR, _IR, _IRBytes);
	m_vRecording.push_back(Tmp);

	// Save the number of IR bytes
	IRBytes = _IRBytes;

	// The upper limit of a recording coincides with the IniFile.cpp limit, each list element
	// is 7 bytes, therefore be divide by 7
	if (m_vRecording.size() > (10*1024 / 7 - 2) )
	{
		m_bRecording = false;
		m_RecordButton[m_iRecordTo]->SetLabel(wxT("Done"));
		ConvertToString();
		UpdateRecordingGUI();
	}
}

