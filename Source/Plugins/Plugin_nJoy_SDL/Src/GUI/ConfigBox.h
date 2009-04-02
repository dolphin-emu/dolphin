//////////////////////////////////////////////////////////////////////////////////////////
// Project description
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
// Name: nJoy 
// Description: A Dolphin Compatible Input Plugin
//
// Author: Falcon4ever (nJoy@falcon4ever.com)
// Site: www.multigesture.net
// Copyright (C) 2003-2008 Dolphin Project.
//
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


#ifndef __CONFIGBOX_h__
#define __CONFIGBOX_h__

#ifndef WX_PRECOMP
	#include <wx/wx.h>
	#include <wx/dialog.h>
#else
	#include <wx/wxprec.h>
#endif

#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/statbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/statbmp.h>
#include <wx/gbsizer.h>

#include "../nJoy.h"

class ConfigBox : public wxDialog
{
	private:
		DECLARE_EVENT_TABLE();
		
	public:
		ConfigBox(wxWindow *parent, wxWindowID id = 1,
			const wxString &title = wxT("Configure: nJoy Input Plugin"),
			const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
			long style = wxDEFAULT_DIALOG_STYLE);
		virtual ~ConfigBox();

	#if wxUSE_TIMER
		void OnTimer(wxTimerEvent& WXUNUSED(event)) { Update(); }
		void OnButtonTimer(wxTimerEvent& WXUNUSED(event)) { DoGetButtons(GetButtonWaitingID); }
		wxTimer *m_ConstantTimer, *m_ButtonMappingTimer;
	#endif

		// Debugging
		wxStaticText* m_pStatusBar, * m_pStatusBar2;
		wxTextCtrl* m_TCDebugging;
		bool Debugging;
		void LogMsg(const char* format, ...);

		// Status window
		int BoxW, BoxH;

		// Configure buttons
		int GetButtonWaitingID, GetButtonWaitingTimer;
		
	private:
		wxButton *m_About;
		wxButton *m_OK;
		wxButton *m_Cancel;
				
		wxPanel *m_Controller[4]; // Main containers	
		wxNotebook *m_Notebook;
		wxBoxSizer * m_MainSizer;

		wxPanel * m_pKeys[4], * m_pInStatus[4], * m_pOutStatus[4];
		wxBitmap WxStaticBitmap1_BITMAP, WxStaticBitmap1_BITMAPGray;
		wxStaticBoxSizer * m_sKeys[4];
		wxBoxSizer *m_sMain[4], *m_sMainLeft[4], *m_sMainRight[4];

		/////////////////////////////
		// Settings
		// ¯¯¯¯¯¯¯¯¯

		wxComboBox *m_Joyname[4];
		wxComboBox *m_ControlType[4], *m_TriggerType[4];
		wxComboBox *m_Deadzone[4];
		
		wxCheckBox *m_Joyattach[4]; // Attached pad
		wxStaticBoxSizer *m_gJoyname[4];

		wxStaticBoxSizer *m_gExtrasettings[4];  // Extra settings
		wxGridBagSizer * m_gGBExtrasettings[4];

		wxBoxSizer* m_sSettings[4]; // General settings 2
		wxStaticBoxSizer *m_gGenSettings[4];		

		wxStaticBoxSizer *m_gGenSettingsID[4];
		wxGridBagSizer *m_gGBGenSettings[4];
		wxCheckBox *m_CBSaveByID[4], *m_CBShowAdvanced[4];
		wxStaticText *m_TSControltype[4], *m_TSTriggerType[4];

		wxStaticBoxSizer *m_gStatusIn[4], *m_gStatusInSettings[4], *m_gStatusAdvancedSettings[4]; // Advanced settings
		wxBoxSizer *m_gStatusInSettingsH[4];
		wxGridBagSizer *m_GBAdvancedMainStick[4];
		wxStaticText *m_TStatusIn[4], *m_TStatusOut[4], *m_STDiagonal[4];
		wxComboBox *m_CoBDiagonal[4]; wxCheckBox *m_CBS_to_C[4];
		wxCheckBox *m_CBCheckFocus[4], *m_AdvancedMapFilter[4];
		
		wxCheckBox *m_Rumble[4];	// Rumble settings
		wxComboBox *m_RStrength[4]; 
		wxStaticBoxSizer *m_gRumble[4];
		wxGridBagSizer *m_gGBRumble[4];

		wxStaticBoxSizer *m_gStatusTriggers[4]; // Triggers
		wxStaticText *m_TStatusTriggers[4];

		/////////////////////////////
		// Keys
		// ¯¯¯¯¯¯¯¯¯
		int g_Pressed; // Keyboard input

		wxTextCtrl *m_JoyShoulderL[4];
		wxTextCtrl *m_JoyShoulderR[4];

		wxButton *m_bJoyShoulderL[4];
		wxButton *m_bJoyShoulderR[4];

		wxTextCtrl *m_JoyButtonA[4];
		wxTextCtrl *m_JoyButtonB[4];
		wxTextCtrl *m_JoyButtonX[4];
		wxTextCtrl *m_JoyButtonY[4];
		wxTextCtrl *m_JoyButtonZ[4];
		wxTextCtrl *m_JoyButtonStart[4];
		wxTextCtrl *m_JoyButtonHalfpress[4];

		wxButton *m_bJoyButtonA[4];
		wxButton *m_bJoyButtonB[4];
		wxButton *m_bJoyButtonX[4];
		wxButton *m_bJoyButtonY[4];
		wxButton *m_bJoyButtonZ[4];
		wxButton *m_bJoyButtonStart[4];
		wxButton *m_bJoyButtonHalfpress[4];

		wxTextCtrl *m_JoyAnalogMainX[4];
		wxTextCtrl *m_JoyAnalogMainY[4];
		wxTextCtrl *m_JoyAnalogSubX[4];
		wxTextCtrl *m_JoyAnalogSubY[4];

		wxButton *m_bJoyAnalogMainX[4];
		wxButton *m_bJoyAnalogMainY[4];
		wxButton *m_bJoyAnalogSubX[4];
		wxButton *m_bJoyAnalogSubY[4];

		wxTextCtrl *m_JoyDpadUp[4];
		wxTextCtrl *m_JoyDpadDown[4];
		wxTextCtrl *m_JoyDpadLeft[4];
		wxTextCtrl *m_JoyDpadRight[4];

		wxButton *m_bJoyDpadUp[4];
		wxButton *m_bJoyDpadDown[4];
		wxButton *m_bJoyDpadLeft[4];
		wxButton *m_bJoyDpadRight[4];

		wxStaticText *m_textMainX[4];
		wxStaticText *m_textMainY[4];
		wxStaticText *m_textDpadUp[4];
		wxStaticText *m_textDpadDown[4];
		wxStaticText *m_textDpadLeft[4];
		wxStaticText *m_textDpadRight[4];
		wxStaticText *m_textDeadzone[4];
		wxStaticText *m_textHalfpress[4];
		wxStaticText *m_textSubX[4];
		wxStaticText *m_textSubY[4];
		wxStaticText *m_textWebsite[4];
		
		wxTextCtrl *m_PlaceholderBMP[4];
		wxStaticBitmap *m_controllerimage[4],
			*m_bmpSquare[4], *m_bmpDot[4], *m_bmpSquareOut[4], *m_bmpDotOut[4];
		
		int notebookpage; bool ControlsCreated;
#ifdef RERECORDING
		wxStaticBoxSizer *m_SizeRecording[4];
		wxCheckBox *m_CheckRecording[4], *m_CheckPlayback[4];
		wxButton *m_BtnSaveRecording[4];
#endif

	private:
		enum
		{
			ID_ABOUT = 1000,
			ID_OK,
			ID_CANCEL,
			ID_NOTEBOOK,
			ID_CONTROLLERPAGE1,
			ID_CONTROLLERPAGE2,
			ID_CONTROLLERPAGE3,
			ID_CONTROLLERPAGE4,
			ID_CONTROLLERPICTURE, // Background picture

			ID_KEYSPANEL1, ID_KEYSPANEL2, ID_KEYSPANEL3, ID_KEYSPANEL4,

			IDG_JOYSTICK, IDC_JOYNAME, IDC_JOYATTACH, // Controller attached

			IDG_EXTRASETTINGS, IDC_DEADZONE, // Extra settings

			IDG_CONTROLLERTYPE,	IDC_CONTROLTYPE, IDC_TRIGGERTYPE, // Controller type		

			IDC_SAVEBYID, IDC_SHOWADVANCED, // Settings

			IDC_ENABLERUMBLE, IDC_RUMBLESTRENGTH, IDT_RUMBLESTRENGTH, // Rumble
			
			ID_INSTATUS1, ID_INSTATUS2, ID_INSTATUS3, ID_INSTATUS4, // Advanced status
			ID_STATUSBMP1, ID_STATUSBMP2, ID_STATUSBMP3, ID_STATUSBMP4,
			ID_STATUSDOTBMP1, ID_STATUSDOTBMP2, ID_STATUSDOTBMP3, ID_STATUSDOTBMP4,
			IDT_STATUS_IN, IDT_STATUS_OUT,

			// Advaced settings
			IDCB_MAINSTICK_DIAGONAL, IDCB_MAINSTICK_S_TO_C, IDT_MAINSTICK_DIAGONAL, IDT_TRIGGERS, IDCB_CHECKFOCUS, IDCB_FILTER_SETTINGS,
#ifdef RERECORDING
			ID_RECORDING, ID_PLAYBACK, ID_SAVE_RECORDING,
#endif

			// Timers
			IDTM_CONSTANT, IDTM_BUTTON,


			// ==============================================
			// Keys objects
			// -----------------------------

			// -------------------------------------
			// Text controls that hold the mapped key value
			// ----------
			ID_ANALOG_MAIN_X = 2000,
			ID_ANALOG_MAIN_Y,
			ID_ANALOG_SUB_X,
			ID_ANALOG_SUB_Y,
			ID_SHOULDER_L,
			ID_SHOULDER_R,

			ID_DPAD_UP,
			ID_DPAD_DOWN,
			ID_DPAD_LEFT,
			ID_DPAD_RIGHT,

			ID_BUTTON_A,
			ID_BUTTON_B,
			ID_BUTTON_X,
			ID_BUTTON_Y,
			ID_BUTTON_Z,
			ID_BUTTONSTART,

			ID_BUTTONHALFPRESS,
			// ------------------ Keep this order

			// -------------------------------------
			// Buttons controls (it's important that they are kept in this order)	
			// --------
			IDB_ANALOG_MAIN_X = 3000,
			IDB_ANALOG_MAIN_Y,
			IDB_ANALOG_SUB_X,
			IDB_ANALOG_SUB_Y,
			IDB_SHOULDER_L,
			IDB_SHOULDER_R,

			IDB_DPAD_UP,
			IDB_DPAD_DOWN,
			IDB_DPAD_LEFT,
			IDB_DPAD_RIGHT,

			IDB_BUTTON_A,
			IDB_BUTTON_B,
			IDB_BUTTON_X,
			IDB_BUTTON_Y,
			IDB_BUTTON_Z,
			IDB_BUTTONSTART,

			IDB_BUTTONHALFPRESS,
			// ------------------ Keep this order

			// Statis text controls that hold the button label
			IDT_ANALOG_MAIN_X = 4000,
			IDT_ANALOG_MAIN_Y,
			IDT_ANALOG_SUB_X,
			IDT_ANALOG_SUB_Y,

			IDT_DPAD_UP,
			IDT_DPAD_DOWN,
			IDT_DPAD_LEFT,
			IDT_DPAD_RIGHT,
			IDT_DEADZONE,
			IDT_BUTTONHALFPRESS,

			IDT_DPADTYPE, IDT_TRIGGERTYPE,	
			IDT_WEBSITE,
			IDT_DEBUGGING, IDT_DEBUGGING2, IDT_DEBUGGING3,
			// ============

			ID_DUMMY_VALUE_ //don't remove this value unless you have other enum values
		};
	
	private:
		void AboutClick(wxCommandEvent& event);
		void OKClick(wxCommandEvent& event);
		void CancelClick(wxCommandEvent& event);
		void DoSave(bool ChangePad = false, int Slot = -1);

		void DoChangeJoystick(); 

		void PadOpen(int Open); 
		void PadClose(int Close);

		void UpdateGUI(int _notebookpage);

		void ChangeSettings(wxCommandEvent& event);
		void ComboChange(wxCommandEvent& event);

		void OnClose(wxCloseEvent& event);
		void CreateGUIControls();
		void CreateAdvancedControls(int i);
		void SizeWindow();
		wxBitmap CreateBitmap(); 
		wxBitmap CreateBitmapDot();
		void PadGetStatus(); 
		void Update();
 
		void UpdateGUIButtonMapping(int controller);
		void SaveButtonMapping(int controller, bool DontChangeId = false, int FromSlot = -1);
		void SaveButtonMappingAll(int Slot);
		void UpdateGUIAll(int Slot);
		void ToBlank(bool ToBlank = true);
		void OnSaveById();

		void NotebookPageChanged(wxNotebookEvent& event);

		void GetButtons(wxCommandEvent& event); void DoGetButtons(int);
		void GetHats(int ID);
		void GetAxis(wxCommandEvent& event);

		void OnPaint(wxPaintEvent &event);

		void SetButtonText(int id, const char *text, int Page = -1);
		void SetButtonTextAll(int id, const char *text);
		wxString GetButtonText(int id, int Page = -1);
		void OnKeyDown(wxKeyEvent& event);
};

#endif
