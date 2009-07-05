// Copyright (C) 2003-2009 Dolphin Project.

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

#ifndef __BASICCONFIGDIALOG_h__
#define __BASICCONFIGDIALOG_h__

#include <iostream>
#include <vector>

#include <wx/wx.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/combobox.h>
#include <wx/checkbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/gbsizer.h>

class WiimoteBasicConfigDialog : public wxDialog
{
	public:
		WiimoteBasicConfigDialog(wxWindow *parent,
			wxWindowID id = 1,
			const wxString &title = wxT("Wii Remote Plugin Configuration"),
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxDEFAULT_DIALOG_STYLE | wxWANTS_CHARS);
		virtual ~WiimoteBasicConfigDialog();

		// General open, close and event functions
		void ButtonClick(wxCommandEvent& event);
		void UpdateGUI(int Slot = 0);
		void UpdateIRCalibration();
		void ShutDown(wxTimerEvent& WXUNUSED(event));
		void UpdateOnce(wxTimerEvent& event);
		
		// Timers
		wxTimer	*m_TimeoutOnce,
				*m_ShutDownTimer;
		wxCheckBox *m_UseRealWiimote[4];

		bool Closing;
	private:
		DECLARE_EVENT_TABLE();

		bool ControlsCreated,
			 m_bEnableUseRealWiimote;
		int Page;

		wxNotebook *m_Notebook;
		wxPanel *m_Controller[4];
		wxButton *m_Close,
				 *m_Apply,
				 *m_ButtonMapping,
				 *m_Recording;

		wxBoxSizer  *m_MainSizer,
					*m_sMain[4],
					*m_SizeParent[4];

		wxSlider *m_SliderWidth[4],
				 *m_SliderHeight[4],
				 *m_SliderLeft[4],
				 *m_SliderTop[4];

		// Emulated Wiimote settings
		wxCheckBox  *m_SidewaysDPad[4],
					*m_WiimoteOnline[4],
					*m_WiiMotionPlusConnected[4],
					*m_CheckAR43[4],
					*m_CheckAR169[4],
					*m_Crop[4];

		wxStaticText *m_TextScreenWidth[4],
					 *m_TextScreenHeight[4],
					 *m_TextScreenLeft[4],
					 *m_TextScreenTop[4],
					 *m_TextAR[4];
		wxBoxSizer  *m_SizeBasicGeneral[4],
					*m_SizeBasicGeneralLeft[4],
					*m_SizeBasicGeneralRight[4],			
					*m_SizerIRPointerWidth[4],
					*m_SizerIRPointerHeight[4],
					*m_SizerIRPointerScreen[4];

		wxStaticBoxSizer *m_SizeBasic[4],
						 *m_SizeEmu[4],
						 *m_SizeReal[4],
						 *m_SizeExtensions[4],
						 *m_SizerIRPointer[4];

		wxChoice* extensionChoice[4];

		// Real Wiimote settings
		wxCheckBox  *m_ConnectRealWiimote[4];

		enum
		{
			ID_CLOSE = 1000,
			ID_APPLY,
			ID_BUTTONMAPPING,
			ID_BUTTONRECORDING,
			IDTM_SHUTDOWN,
			IDTM_UPDATE,
			IDTM_UPDATE_ONCE,
			
			ID_NOTEBOOK,
			ID_CONTROLLERPAGE1,
			ID_CONTROLLERPAGE2,
			ID_CONTROLLERPAGE3,
			ID_CONTROLLERPAGE4,
			
			// Emulated Wiimote
			ID_SIDEWAYSDPAD,
			ID_MOTIONPLUSCONNECTED,
			ID_EXTCONNECTED,
			IDC_WIMOTE_ON,
			
			IDS_WIDTH,			
			IDS_HEIGHT, 
			IDS_LEFT,
			IDS_TOP,

			// Real
			ID_CONNECT_REAL,
			ID_USE_REAL,
		};

		void OnClose(wxCloseEvent& event);
		void CreateGUIControls();
		void GeneralSettingsChanged(wxCommandEvent& event);
		void IRCursorChanged(wxScrollEvent& event);

		void DoConnectReal(); // Real
		void DoUseReal();

		void DoExtensionConnectedDisconnected(int Extension = -1); // Emulated
};

extern WiimoteBasicConfigDialog *m_BasicConfigFrame;
#endif
