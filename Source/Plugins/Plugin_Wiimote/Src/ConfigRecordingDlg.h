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

#ifndef __CONFIGDIALOG_h__
#define __CONFIGDIALOG_h__

#include <iostream>
#include <vector>

#include <wx/wx.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/combobox.h>
#include <wx/checkbox.h>
#include <wx/panel.h>
#include "Thread.h"

class WiimoteRecordingConfigDialog : public wxDialog
{
	public:
		WiimoteRecordingConfigDialog(wxWindow *parent,
			wxWindowID id = 1,
			const wxString &title = wxT("Wii Remote Plugin Configuration"),
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxDEFAULT_DIALOG_STYLE | wxWANTS_CHARS);
		virtual ~WiimoteRecordingConfigDialog(){;}

		void UpdateRecordingGUI(int Slot = 0);
		void LoadFile();
		void SaveFile();
		void DoRecordMovement(int _x, int _y, int _z, const u8 *_IR, int IRBytes);
		void DoRecordA(bool Pressed);
		void ConvertToString();

		void CloseClick(wxCommandEvent& event);
		void RecordMovement(wxCommandEvent& event);
		void Update(wxTimerEvent& WXUNUSED(event));

		bool m_bWaitForRecording,
			 m_bRecording;

		int m_iRecordTo;

		wxTimer *m_TimeoutTimer;

		// General status
		wxStaticText * m_TextUpdateRate,
					 *m_TextIR;

		// Wiimote status
		wxGauge *m_GaugeBattery,
				*m_GaugeRoll[2],
				*m_GaugeGForce[3],
				*m_GaugeAccel[3],
				*m_GaugeAccelNunchuk[3],
				*m_GaugeGForceNunchuk[3];

	private:
		DECLARE_EVENT_TABLE();

		bool ControlsCreated;
		THREAD_RETURN SafeCloseReadWiimote_ThreadFunc2(void* arg);
		Common::Thread*		g_pReadThread2;

		wxPanel *m_PageRecording;
		wxButton *m_Close,
				 *m_Apply;

		wxBoxSizer  *m_MainSizer,
					*m_sRecordingMain;

		wxCheckBox *m_UpdateMeters;

		wxButton * m_RecordButton[RECORDING_ROWS + 1];
		wxChoice * m_RecordHotKeySwitch[RECORDING_ROWS + 1];
		wxChoice * m_RecordHotKeyWiimote[RECORDING_ROWS + 1];
		wxChoice * m_RecordHotKeyNunchuck[RECORDING_ROWS + 1];
		wxChoice * m_RecordHotKeyIR[RECORDING_ROWS + 1];
		wxTextCtrl * m_RecordText[RECORDING_ROWS + 1];
		wxTextCtrl * m_RecordGameText[RECORDING_ROWS + 1];
		wxTextCtrl * m_RecordIRBytesText[RECORDING_ROWS + 1];
		wxTextCtrl * m_RecordSpeed[RECORDING_ROWS + 1];
		wxChoice * m_RecordPlayBackSpeed[RECORDING_ROWS + 1];

		std::vector<SRecording> m_vRecording;
		int IRBytes;

		enum
		{
			ID_CLOSE = 1000,
			ID_APPLY,
			ID_RECORDINGPAGE,
			IDTM_UPDATE,

			// Real
			ID_UPDATE_REAL,
			IDB_RECORD = 2000,
			IDC_RECORD = 3000,
			IDC_PLAY_WIIMOTE,
			IDC_PLAY_NUNCHUCK,
			IDC_PLAY_IR,
			IDT_RECORD_TEXT,
			IDT_RECORD_GAMETEXT,
			IDT_RECORD_IRBYTESTEXT,
			IDT_RECORD_SPEED,
			IDT_RECORD_PLAYSPEED
		};

		void OnClose(wxCloseEvent& event);
		void CreateGUIControlsRecording();
		void RecordingChanged(wxCommandEvent& event);
};

extern WiimoteRecordingConfigDialog *m_RecordingConfigFrame;
#endif
