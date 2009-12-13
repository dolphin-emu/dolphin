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

		void CloseClick(wxCommandEvent& event);
		void UpdateGUI(int Slot = 0);
		void UpdateGUIButtonMapping(int controller);
		void UpdateControls();
		void OnKeyDown(wxKeyEvent& event);
		void Convert2Box(int &x);
		void ConvertToString();
		void OnButtonTimer(wxTimerEvent& WXUNUSED(event)) { DoGetButtons(GetButtonWaitingID); }
		void UpdatePad(wxTimerEvent& WXUNUSED(event));

		wxTimer *m_UpdatePadTimer,
				*m_ButtonMappingTimer;

		wxStaticBitmap *m_bmpDotLeftIn[4],
			*m_bmpDotLeftOut[4],
			*m_bmpDotRightIn[4],
			*m_bmpDotRightOut[4],
			*m_bmpDeadZoneLeftIn[4],
			*m_bmpDeadZoneRightIn[4];

	private:
		DECLARE_EVENT_TABLE();

		bool ControlsCreated;
		int Page, g_Pressed, BoxW, BoxH;

		wxString OldLabel;

		wxNotebook *m_Notebook;

		wxPanel *m_Controller[4],
			*m_pLeftInStatus[4],
			*m_pLeftOutStatus[4],
			*m_pRightInStatus[4],
			*m_pRightOutStatus[4];

		wxStaticBitmap *m_bmpSquareLeftIn[4],
			*m_bmpSquareLeftOut[4],
			*m_bmpSquareRightIn[4],
			*m_bmpSquareRightOut[4];
	

		wxCheckBox *m_CheckC2S[4],
			*m_CheckRumble[4],
			*m_TiltInvertRoll[4],
			*m_TiltInvertPitch[4];

		wxButton *m_Close, *m_Apply, *ClickedButton,
			*m_Button_Analog[AN_CONTROLS][4],
			*m_Button_Wiimote[WM_CONTROLS][4],
			*m_Button_NunChuck[NC_CONTROLS][4],
			*m_Button_Classic[CC_CONTROLS][4],
			*m_Button_GH3[GH3_CONTROLS][4];

		wxComboBox *m_Joyname[4],
			*m_ComboDeadZoneLeft[4],
			*m_ComboDeadZoneRight[4],
			*m_ComboDiagonal[4],
			*m_RumbleStrength[4],
			*m_TiltComboInput[4],
			*m_TiltComboRangeRoll[4],
			*m_TiltComboRangePitch[4],
			*m_TriggerType[4],
			*m_NunchuckComboStick[4],
			*m_CcComboLeftStick[4],
			*m_CcComboRightStick[4],
			*m_CcComboTriggers[4],
			*m_GH3ComboAnalog[4];

		wxGridBagSizer *m_sGridTilt[4], 
			*m_sGridStickLeft[4],
			*m_sGridStickRight[4],
			*m_sGridTrigger[4];

		wxBoxSizer  *m_MainSizer,
					*m_sMain[4],
					*m_sDeadZoneHoriz[4],
					*m_sDeadZone[4],
					*m_sDiagonal[4],
					*m_sCircle2Square[4],
					*m_sC2SDeadZone[4],
					*m_sJoyname[4],
					*m_sRumble[4],
					*m_sHorizController[4],
					*m_sHorizStatus[4],
					*m_Sizer_Analog[AN_CONTROLS][4],
					*m_sAnalogLeft[4],
					*m_sAnalogMiddle[4],
					*m_sAnalogRight[4],
					*m_sHorizAnalogMapping[4],
					*m_Sizer_Wiimote[WM_CONTROLS][4],
					*m_sWmVertLeft[4],
					*m_sWmVertRight[4],
					*m_Sizer_NunChuck[NC_CONTROLS][4],
					*m_sNunchuckStick[4],
					*m_Sizer_Classic[CC_CONTROLS][4],
					*m_sCcLeftStick[4],
					*m_sCcRightStick[4],
					*m_sCcTriggers[4],
					*m_sCcVertLeft[4],
					*m_sCcVertMiddle[4],
					*m_sCcVertRight[4],
					*m_Sizer_GH3[GH3_CONTROLS][4],
					*m_sGH3Analog[4],
					*m_sGH3VertLeft[4],
					*m_sGH3VertRight[4],
					*m_sHorizControllerMapping[4];

		wxStaticBoxSizer *m_gJoyPad[4],
			*m_gTilt[4],
			*m_gStickLeft[4],
			*m_gStickRight[4],
			*m_gTriggers[4],
			*m_gAnalog[4],
			*m_gWiimote[4],
			*m_gNunchuck[4],
			*m_gClassicController[4],
			*m_gGuitarHero3Controller[4];

		wxStaticText *m_ComboDeadZoneLabel[4],
			*m_DiagonalLabel[4],
			*m_RumbleStrengthLabel[4],
			*m_TiltTextRoll[4], *m_TiltTextPitch[4],
			*m_tStatusLeftIn[4], *m_tStatusLeftOut[4], *m_tStatusRightIn[4], *m_tStatusRightOut[4],
			*m_TriggerL[4], *m_TriggerR[4],
			*m_TriggerStatusL[4], *m_TriggerStatusR[4],
			*m_tTriggerSource[4],
			*m_statictext_Analog[AN_CONTROLS][4],
			*m_statictext_Wiimote[WM_CONTROLS][4],
			*m_statictext_NunChuck[NC_CONTROLS][4],
			*m_statictext_Classic[CC_CONTROLS][4],
			*m_statictext_GH3[GH3_CONTROLS][4],
			*m_NunchuckTextStick[5],
			*m_CcTextLeftStick[4],
			*m_CcTextRightStick[4],
			*m_CcTextTriggers[4],
			*m_tGH3Analog[4];

		enum
		{
			ID_CLOSE = 1000,
			ID_APPLY,
			IDTM_BUTTON, // Timer
			IDTM_UPDATE_PAD, // Timer

			ID_NOTEBOOK,
			ID_CONTROLLERPAGE1,
			ID_CONTROLLERPAGE2,
			ID_CONTROLLERPAGE3,
			ID_CONTROLLERPAGE4,

			// Gamepad <It's important that the internal ordering of these are unchanged>
			IDB_ANALOG_LEFT_X, IDB_ANALOG_LEFT_Y,
			IDB_ANALOG_RIGHT_X, IDB_ANALOG_RIGHT_Y,
			IDB_TRIGGER_L, IDB_TRIGGER_R,

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

			// Gamepad settings
			IDC_JOYNAME,
			IDC_RUMBLE, IDC_RUMBLE_STRENGTH,
			IDC_DEAD_ZONE_LEFT, IDC_DEAD_ZONE_RIGHT,
			IDC_STICK_DIAGONAL, IDC_STICK_C2S,
			IDC_TILT_INPUT,
			IDC_TILT_RANGE_ROLL, IDC_TILT_RANGE_PITCH,
			IDC_TILT_INVERT_ROLL, IDC_TILT_INVERT_PITCH,
			IDC_TRIGGER_TYPE, 
			IDC_NUNCHUCK_STICK,
			IDC_CC_LEFT_STICK, IDC_CC_RIGHT_STICK, IDC_CC_TRIGGERS,
			IDC_GH3_ANALOG,
		};

		wxBitmap CreateBitmap();
		wxBitmap CreateBitmapDot();
		wxBitmap CreateBitmapDeadZone(int Radius);
		wxBitmap CreateBitmapClear();

		void OnClose(wxCloseEvent& event);
		void CreatePadGUIControls();
		void GeneralSettingsChanged(wxCommandEvent& event);

		// Gamepad configuration
		void SetButtonText(int id, const char text[128], int _Page = -1);
		void SetButtonTextAll(int id, char text[128]);
		void GetButtons(wxCommandEvent& btn_event);
		void DoGetButtons(int id);
		void SaveButtonMapping(int controller, bool DontChangeId = false, int FromSlot = -1);
		void SaveButtonMappingAll(int Slot);
		void SaveKeyboardMapping(int Controller, int Id, int Key);
		void ToBlank(bool ToBlank = true);
		void PadGetStatus();
		void DoSave(bool ChangePad = false, int Slot = -1);
		void DoChangeJoystick();
		void PadOpen(int Open); void PadClose(int Close);
		void DoChangeDeadZone(bool Left);
		void OnButtonClick(wxCommandEvent& event);

		// Configure buttons
		int GetButtonWaitingID, GetButtonWaitingTimer;
		wxString GetButtonText(int id, int Page = -1);
};
extern WiimotePadConfigDialog *m_PadConfigFrame;
#endif
