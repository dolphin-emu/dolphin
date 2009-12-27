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

#ifndef __PADCONFIGDIALOG_h__
#define __PADCONFIGDIALOG_h__

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
#include "Config.h"
#include "wiimote_hid.h"

#if defined(HAVE_X11) && HAVE_X11
	#include <X11/Xlib.h>
	#include <X11/Xutil.h>
	#include <X11/keysym.h>
	#include <X11/XKBlib.h>
	#include "X11InputBase.h"
#endif

class WiimotePadConfigDialog : public wxDialog
{
	public:
		WiimotePadConfigDialog(wxWindow *parent,
			wxWindowID id = 1,
			const wxString &title = wxT("Wii Remote Plugin Configuration"),
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxDEFAULT_DIALOG_STYLE | wxWANTS_CHARS);
		virtual ~WiimotePadConfigDialog();

		void Convert2Box(int &x);

		wxTimer *m_UpdatePadTimer,
				*m_ButtonMappingTimer;

		wxStaticBitmap *m_bmpDotLeftIn[MAX_WIIMOTES],
			*m_bmpDotLeftOut[MAX_WIIMOTES],
			*m_bmpDotRightIn[MAX_WIIMOTES],
			*m_bmpDotRightOut[MAX_WIIMOTES],
			*m_bmpDeadZoneLeftIn[MAX_WIIMOTES],
			*m_bmpDeadZoneRightIn[MAX_WIIMOTES];

	private:
		DECLARE_EVENT_TABLE();

		enum
		{
			// <It's important that the internal ordering of these are unchanged>
			// Wiimote
			IDB_WM_A, IDB_WM_B,
			IDB_WM_1, IDB_WM_2,
			IDB_WM_P, IDB_WM_M, IDB_WM_H,
			IDB_WM_L, IDB_WM_R, IDB_WM_U, IDB_WM_D,
			IDB_WM_ROLL_L, IDB_WM_ROLL_R,
			IDB_WM_PITCH_U, IDB_WM_PITCH_D,
			IDB_WM_SHAKE,

			// Nunchuck
			IDB_NC_Z,
			IDB_NC_C,
			IDB_NC_L,
			IDB_NC_R,
			IDB_NC_U,
			IDB_NC_D,
			IDB_NC_ROLL_L, IDB_NC_ROLL_R,
			IDB_NC_PITCH_U, IDB_NC_PITCH_D,
			IDB_NC_SHAKE,

			// Classic Controller
			IDB_CC_A, IDB_CC_B,
			IDB_CC_X, IDB_CC_Y,
			IDB_CC_P, IDB_CC_M, IDB_CC_H,
			IDB_CC_TL, IDB_CC_TR, IDB_CC_ZL, IDB_CC_ZR, // Shoulder triggers and Zs
			IDB_CC_DL, IDB_CC_DR, IDB_CC_DU, IDB_CC_DD, // Digital pad
			IDB_CC_LL, IDB_CC_LR, IDB_CC_LU, IDB_CC_LD, // Left analog stick
			IDB_CC_RL, IDB_CC_RR, IDB_CC_RU, IDB_CC_RD, // Right analog stick

			// Guitar Hero 3 Controller
			IDB_GH3_GREEN,
			IDB_GH3_RED,
			IDB_GH3_YELLOW,
			IDB_GH3_BLUE,
			IDB_GH3_ORANGE,
			IDB_GH3_PLUS,
			IDB_GH3_MINUS,
			IDB_GH3_WHAMMY,
			IDB_GH3_ANALOG_LEFT,
			IDB_GH3_ANALOG_RIGHT,
			IDB_GH3_ANALOG_UP,
			IDB_GH3_ANALOG_DOWN,
			IDB_GH3_STRUM_UP,
			IDB_GH3_STRUM_DOWN,

			// Gamepad
			IDB_ANALOG_LEFT_X, IDB_ANALOG_LEFT_Y,
			IDB_ANALOG_RIGHT_X, IDB_ANALOG_RIGHT_Y,
			IDB_TRIGGER_L, IDB_TRIGGER_R,

			ID_CLOSE = 1000,
			ID_APPLY,
			IDTM_BUTTON, // Timer
			IDTM_UPDATE_PAD, // Timer

			ID_NOTEBOOK,
			ID_CONTROLLERPAGE1,
			ID_CONTROLLERPAGE2,
			ID_CONTROLLERPAGE3,
			ID_CONTROLLERPAGE4,

			// Gamepad settings
			IDC_JOYNAME,
			IDC_RUMBLE, IDC_RUMBLE_STRENGTH,
			IDC_DEAD_ZONE_LEFT, IDC_DEAD_ZONE_RIGHT,
			IDC_STICK_DIAGONAL, IDC_STICK_C2S,
			IDC_TILT_TYPE_WM, IDC_TILT_TYPE_NC,
			IDC_TILT_ROLL, IDC_TILT_ROLL_SWING,
			IDC_TILT_PITCH, IDC_TILT_PITCH_SWING,
			IDC_TILT_ROLL_INVERT, IDC_TILT_PITCH_INVERT,
			IDC_TRIGGER_TYPE, 
			IDC_NUNCHUCK_STICK,
			IDC_CC_LEFT_STICK, IDC_CC_RIGHT_STICK, IDC_CC_TRIGGERS,
			IDC_GH3_ANALOG,
		};

		bool ControlsCreated;
		int m_Page, BoxW, BoxH;

		wxString OldLabel;

		wxNotebook *m_Notebook;

		wxPanel *m_Controller[MAX_WIIMOTES],
			*m_pLeftInStatus[MAX_WIIMOTES],
			*m_pLeftOutStatus[MAX_WIIMOTES],
			*m_pRightInStatus[MAX_WIIMOTES],
			*m_pRightOutStatus[MAX_WIIMOTES];

		wxStaticBitmap *m_bmpSquareLeftIn[MAX_WIIMOTES],
			*m_bmpSquareLeftOut[MAX_WIIMOTES],
			*m_bmpSquareRightIn[MAX_WIIMOTES],
			*m_bmpSquareRightOut[MAX_WIIMOTES];
	
		wxCheckBox *m_CheckC2S[MAX_WIIMOTES],
			*m_CheckRumble[MAX_WIIMOTES],
			*m_TiltRollSwing[MAX_WIIMOTES],
			*m_TiltPitchSwing[MAX_WIIMOTES],
			*m_TiltRollInvert[MAX_WIIMOTES],
			*m_TiltPitchInvert[MAX_WIIMOTES];

		wxButton *m_Close, *m_Apply, *ClickedButton,
			*m_Button_Analog[IDB_TRIGGER_R - IDB_ANALOG_LEFT_X + 1][MAX_WIIMOTES],
			*m_Button_Wiimote[IDB_WM_SHAKE - IDB_WM_A + 1][MAX_WIIMOTES],
			*m_Button_NunChuck[IDB_NC_SHAKE - IDB_NC_Z + 1][MAX_WIIMOTES],
			*m_Button_Classic[IDB_CC_RD - IDB_CC_A + 1][MAX_WIIMOTES],
			*m_Button_GH3[IDB_GH3_STRUM_DOWN - IDB_GH3_GREEN + 1][MAX_WIIMOTES];

		wxComboBox *m_Joyname[MAX_WIIMOTES],
			*m_ComboDeadZoneLeft[MAX_WIIMOTES],
			*m_ComboDeadZoneRight[MAX_WIIMOTES],
			*m_ComboDiagonal[MAX_WIIMOTES],
			*m_RumbleStrength[MAX_WIIMOTES],
			*m_TiltTypeWM[MAX_WIIMOTES],
			*m_TiltTypeNC[MAX_WIIMOTES],
			*m_TiltComboRangeRoll[MAX_WIIMOTES],
			*m_TiltComboRangePitch[MAX_WIIMOTES],
			*m_TriggerType[MAX_WIIMOTES],
			*m_NunchuckComboStick[MAX_WIIMOTES],
			*m_CcComboLeftStick[MAX_WIIMOTES],
			*m_CcComboRightStick[MAX_WIIMOTES],
			*m_CcComboTriggers[MAX_WIIMOTES],
			*m_GH3ComboAnalog[MAX_WIIMOTES];

		wxGridBagSizer *m_sGridTilt[MAX_WIIMOTES], 
			*m_sGridStickLeft[MAX_WIIMOTES],
			*m_sGridStickRight[MAX_WIIMOTES],
			*m_sGridTrigger[MAX_WIIMOTES];

		wxBoxSizer  *m_MainSizer,
					*m_sMain[MAX_WIIMOTES],
					*m_sDeadZoneHoriz[MAX_WIIMOTES],
					*m_sDeadZone[MAX_WIIMOTES],
					*m_sDiagonal[MAX_WIIMOTES],
					*m_sCircle2Square[MAX_WIIMOTES],
					*m_sC2SDeadZone[MAX_WIIMOTES],
					*m_sJoyname[MAX_WIIMOTES],
					*m_sRumble[MAX_WIIMOTES],
					*m_sTiltType[MAX_WIIMOTES],
					*m_sHorizController[MAX_WIIMOTES],
					*m_sHorizStatus[MAX_WIIMOTES],
					*m_Sizer_Analog[IDB_TRIGGER_R - IDB_ANALOG_LEFT_X + 1][MAX_WIIMOTES],
					*m_sAnalogLeft[MAX_WIIMOTES],
					*m_sAnalogMiddle[MAX_WIIMOTES],
					*m_sAnalogRight[MAX_WIIMOTES],
					*m_sHorizAnalogMapping[MAX_WIIMOTES],
					*m_Sizer_Wiimote[IDB_WM_SHAKE - IDB_WM_A + 1][MAX_WIIMOTES],
					*m_sWmVertLeft[MAX_WIIMOTES],
					*m_sWmVertRight[MAX_WIIMOTES],
					*m_Sizer_NunChuck[IDB_NC_SHAKE - IDB_NC_Z + 1][MAX_WIIMOTES],
					*m_sNunchuckStick[MAX_WIIMOTES],
					*m_sNCVertLeft[MAX_WIIMOTES],
					*m_sNCVertRight[MAX_WIIMOTES],
					*m_Sizer_Classic[IDB_CC_RD - IDB_CC_A + 1][MAX_WIIMOTES],
					*m_sCcLeftStick[MAX_WIIMOTES],
					*m_sCcRightStick[MAX_WIIMOTES],
					*m_sCcTriggers[MAX_WIIMOTES],
					*m_sCcVertLeft[MAX_WIIMOTES],
					*m_sCcVertMiddle[MAX_WIIMOTES],
					*m_sCcVertRight[MAX_WIIMOTES],
					*m_Sizer_GH3[IDB_GH3_STRUM_DOWN - IDB_GH3_GREEN + 1][MAX_WIIMOTES],
					*m_sGH3Analog[MAX_WIIMOTES],
					*m_sGH3VertLeft[MAX_WIIMOTES],
					*m_sGH3VertRight[MAX_WIIMOTES],
					*m_sHorizControllerMapping[MAX_WIIMOTES];

		wxStaticBoxSizer *m_gJoyPad[MAX_WIIMOTES],
			*m_gTilt[MAX_WIIMOTES],
			*m_gStickLeft[MAX_WIIMOTES],
			*m_gStickRight[MAX_WIIMOTES],
			*m_gTriggers[MAX_WIIMOTES],
			*m_gAnalog[MAX_WIIMOTES],
			*m_gWiimote[MAX_WIIMOTES],
			*m_gNunchuck[MAX_WIIMOTES],
			*m_gClassicController[MAX_WIIMOTES],
			*m_gGuitarHero3Controller[MAX_WIIMOTES];

		wxStaticText *m_ComboDeadZoneLabel[MAX_WIIMOTES],
			*m_DiagonalLabel[MAX_WIIMOTES],
			*m_RumbleStrengthLabel[MAX_WIIMOTES],
			*m_tTiltTypeWM[MAX_WIIMOTES], *m_tTiltTypeNC[MAX_WIIMOTES],
			*m_TiltTextRoll[MAX_WIIMOTES], *m_TiltTextPitch[MAX_WIIMOTES],
			*m_tStatusLeftIn[MAX_WIIMOTES], *m_tStatusLeftOut[MAX_WIIMOTES],
			*m_tStatusRightIn[MAX_WIIMOTES], *m_tStatusRightOut[MAX_WIIMOTES],
			*m_TriggerL[MAX_WIIMOTES], *m_TriggerR[MAX_WIIMOTES],
			*m_TriggerStatusL[MAX_WIIMOTES], *m_TriggerStatusR[MAX_WIIMOTES],
			*m_tTriggerSource[MAX_WIIMOTES],
			*m_statictext_Analog[IDB_TRIGGER_R - IDB_ANALOG_LEFT_X + 1][MAX_WIIMOTES],
			*m_statictext_Wiimote[IDB_WM_SHAKE - IDB_WM_A + 1][MAX_WIIMOTES],
			*m_statictext_NunChuck[IDB_NC_SHAKE - IDB_NC_Z + 1][MAX_WIIMOTES],
			*m_statictext_Classic[IDB_CC_RD - IDB_CC_A + 1][MAX_WIIMOTES],
			*m_statictext_GH3[IDB_GH3_STRUM_DOWN - IDB_GH3_GREEN + 1][MAX_WIIMOTES],
			*m_NunchuckTextStick[5],
			*m_CcTextLeftStick[MAX_WIIMOTES],
			*m_CcTextRightStick[MAX_WIIMOTES],
			*m_CcTextTriggers[MAX_WIIMOTES],
			*m_tGH3Analog[MAX_WIIMOTES];

		wxBitmap CreateBitmap();
		wxBitmap CreateBitmapDot();
		wxBitmap CreateBitmapDeadZone(int Radius);
		wxBitmap CreateBitmapClear();

		void OnClose(wxCloseEvent& event);
		void CloseClick(wxCommandEvent& event);
		void CreatePadGUIControls();
		void UpdateGUI();
		void NotebookPageChanged(wxNotebookEvent& event);
		void OnButtonTimer(wxTimerEvent& WXUNUSED(event)) { DoGetButtons(GetButtonWaitingID); }
		void OnKeyDown(wxKeyEvent& event);
		void OnButtonClick(wxCommandEvent& event);
		void GeneralSettingsChanged(wxCommandEvent& event);
		void SaveButtonMapping(int Id, int Key);

		// Gamepad configuration
		void SetButtonText(int id,const wxString &str);
		wxString GetButtonText(int id);
		void GetButtons(wxCommandEvent& btn_event);
		void DoGetButtons(int id);
		void UpdatePadInfo(wxTimerEvent& WXUNUSED(event));
		void ToBlank(bool ToBlank, int Id);
		void DoChangeDeadZone();

		// Configure buttons
		int GetButtonWaitingID, GetButtonWaitingTimer, g_Pressed;
};
extern WiimotePadConfigDialog *m_PadConfigFrame;
#endif
