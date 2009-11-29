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

#include "wiimote_hid.h"
#include "main.h"
#include "ConfigRecordingDlg.h"
#include "ConfigBasicDlg.h"
#include "Config.h"
#include "EmuMain.h" // for LoadRecordedMovements()

BEGIN_EVENT_TABLE(WiimoteRecordingConfigDialog,wxDialog)
	EVT_CLOSE(WiimoteRecordingConfigDialog::OnClose)
	EVT_BUTTON(ID_CLOSE, WiimoteRecordingConfigDialog::CloseClick)
	EVT_BUTTON(ID_APPLY, WiimoteRecordingConfigDialog::CloseClick)

	EVT_CHOICE(IDC_RECORD + 1, WiimoteRecordingConfigDialog::RecordingChanged)
	EVT_CHOICE(IDC_RECORD + 2, WiimoteRecordingConfigDialog::RecordingChanged)
	EVT_CHOICE(IDC_RECORD + 3, WiimoteRecordingConfigDialog::RecordingChanged)
	EVT_CHOICE(IDC_RECORD + 4, WiimoteRecordingConfigDialog::RecordingChanged)
	EVT_CHOICE(IDC_RECORD + 5, WiimoteRecordingConfigDialog::RecordingChanged)
	EVT_CHOICE(IDC_RECORD + 6, WiimoteRecordingConfigDialog::RecordingChanged)
	EVT_CHOICE(IDC_RECORD + 7, WiimoteRecordingConfigDialog::RecordingChanged)
	EVT_CHOICE(IDC_RECORD + 8, WiimoteRecordingConfigDialog::RecordingChanged)
	EVT_CHOICE(IDC_RECORD + 9, WiimoteRecordingConfigDialog::RecordingChanged)
	EVT_CHOICE(IDC_RECORD + 10, WiimoteRecordingConfigDialog::RecordingChanged)
	EVT_CHOICE(IDC_RECORD + 11, WiimoteRecordingConfigDialog::RecordingChanged)
	EVT_CHOICE(IDC_RECORD + 12, WiimoteRecordingConfigDialog::RecordingChanged)
	EVT_CHOICE(IDC_RECORD + 13, WiimoteRecordingConfigDialog::RecordingChanged)
	EVT_CHOICE(IDC_RECORD + 14, WiimoteRecordingConfigDialog::RecordingChanged)
	EVT_CHOICE(IDC_RECORD + 15, WiimoteRecordingConfigDialog::RecordingChanged)
	EVT_BUTTON(IDB_RECORD + 1, WiimoteRecordingConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 2, WiimoteRecordingConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 3, WiimoteRecordingConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 4, WiimoteRecordingConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 5, WiimoteRecordingConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 6, WiimoteRecordingConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 7, WiimoteRecordingConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 8, WiimoteRecordingConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 9, WiimoteRecordingConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 10, WiimoteRecordingConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 11, WiimoteRecordingConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 12, WiimoteRecordingConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 13, WiimoteRecordingConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 14, WiimoteRecordingConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 15, WiimoteRecordingConfigDialog::RecordMovement)
	EVT_TIMER(IDTM_UPDATE, WiimoteRecordingConfigDialog::Update)
END_EVENT_TABLE()


WiimoteRecordingConfigDialog::WiimoteRecordingConfigDialog(wxWindow *parent, wxWindowID id, const wxString &title,
						   const wxPoint &position, const wxSize& size, long style)
: wxDialog
(parent, id, title, position, size, style)
{
#if wxUSE_TIMER
	m_TimeoutTimer = new wxTimer(this, IDTM_UPDATE);
#endif

	m_bWaitForRecording = false;
	m_bRecording = false;

	m_vRecording.resize(RECORDING_ROWS + 1);

	g_Config.Load();
	CreateGUIControlsRecording();
	SetBackgroundColour(m_PageRecording->GetBackgroundColour());
	LoadFile();
	// Set control values
	UpdateRecordingGUI();
}

void WiimoteRecordingConfigDialog::OnClose(wxCloseEvent& event)
{
	event.Skip();
	g_FrameOpen = false;
	SaveFile();
	EndModal(wxID_CLOSE);
}


void WiimoteRecordingConfigDialog::CloseClick(wxCommandEvent& event)
{
	switch(event.GetId())
	{
	case ID_CLOSE:
		Close();
		break;
	case ID_APPLY:
		SaveFile();
		WiiMoteEmu::LoadRecordedMovements();
		break;
	}
}

void WiimoteRecordingConfigDialog::RecordingChanged(wxCommandEvent& event)
{

	switch (event.GetId())
	{
	case ID_UPDATE_REAL:
		g_Config.bUpdateRealWiimote = m_UpdateMeters->IsChecked();
		break;
	default:
		// Check if any of the other choice boxes has the same hotkey
		for (int i = 1; i < (RECORDING_ROWS + 1); i++)
		{
			int CurrentChoiceBox = (event.GetId() - IDC_RECORD);
			if (i == CurrentChoiceBox) continue;
			if (m_RecordHotKeyWiimote[i]->GetSelection() == m_RecordHotKeyWiimote[CurrentChoiceBox]->GetSelection()) m_RecordHotKeyWiimote[i]->SetSelection(10);
			if (m_RecordHotKeyNunchuck[i]->GetSelection() == m_RecordHotKeyNunchuck[CurrentChoiceBox]->GetSelection()) m_RecordHotKeyNunchuck[i]->SetSelection(10);
			if (m_RecordHotKeyIR[i]->GetSelection() == m_RecordHotKeyIR[CurrentChoiceBox]->GetSelection()) m_RecordHotKeyIR[i]->SetSelection(10);
			
			//DEBUG_LOG(WIIMOTE, "HotKey: %i %i",
			//	m_RecordHotKey[i]->GetSelection(), m_RecordHotKey[CurrentChoiceBox]->GetSelection());
		}
		break;
	}
	g_Config.Save();
	UpdateRecordingGUI();
}


void WiimoteRecordingConfigDialog::UpdateRecordingGUI(int Slot)
{
	// Disable all recording buttons
	bool ActiveRecording = !(m_bWaitForRecording || m_bRecording);
	#ifdef _WIN32
		for(int i = IDB_RECORD + 1; i < (IDB_RECORD + RECORDING_ROWS + 1); i++)
			if(ControlsCreated) m_PageRecording->FindItem(i)->Enable(ActiveRecording);
	#endif
}

