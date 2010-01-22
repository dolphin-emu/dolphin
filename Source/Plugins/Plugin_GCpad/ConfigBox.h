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


#ifndef __GCPAD_CONFIGBOX_h__
#define __GCPAD_CONFIGBOX_h__

#include <wx/wx.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/combobox.h>
#include <wx/checkbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/gbsizer.h>

#include "GCpad.h"

class GCPadConfigDialog : public wxDialog
{	
	public:
		GCPadConfigDialog(wxWindow *parent, wxWindowID id = 1,
			const wxString &title = wxT("Gamecube Pad Plugin Configuration"),
			const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
			long style = wxDEFAULT_DIALOG_STYLE);
		virtual ~GCPadConfigDialog();
		
	private:
		DECLARE_EVENT_TABLE();

		enum
		{
			// it's important that they are kept in this order
			IDB_BTN_A = 0,
			IDB_BTN_B,
			IDB_BTN_X,
			IDB_BTN_Y,
			IDB_BTN_Z,
			IDB_BTN_START,

			IDB_DPAD_UP,
			IDB_DPAD_DOWN,
			IDB_DPAD_LEFT,
			IDB_DPAD_RIGHT,

			IDB_MAIN_UP,
			IDB_MAIN_DOWN,
			IDB_MAIN_LEFT,
			IDB_MAIN_RIGHT,

			IDB_SUB_UP,
			IDB_SUB_DOWN,
			IDB_SUB_LEFT,
			IDB_SUB_RIGHT,

			IDB_SHDR_L,
			IDB_SHDR_R,
			IDB_SHDR_SEMI_L,
			IDB_SHDR_SEMI_R,

			// Joypad
			IDB_ANALOG_LEFT_X, IDB_ANALOG_LEFT_Y,
			IDB_ANALOG_RIGHT_X, IDB_ANALOG_RIGHT_Y,
			IDB_TRIGGER_L, IDB_TRIGGER_R,

			// Dialog controls
			ID_OK = 1000,
			ID_CANCEL,
			ID_NOTEBOOK,
			ID_CONTROLLERPAGE1,
			ID_CONTROLLERPAGE2,
			ID_CONTROLLERPAGE3,
			ID_CONTROLLERPAGE4,

			// Timers
			IDTM_BUTTON, IDTM_UPDATE_PAD,

			// Gamepad settings
			IDC_JOYNAME,
			IDC_DEAD_ZONE_LEFT, IDC_DEAD_ZONE_RIGHT,
			IDC_STICK_DIAGONAL, IDC_STICK_S2C,
			IDC_RUMBLE, IDC_RUMBLE_STRENGTH,
			IDC_TRIGGER_TYPE, 
			IDC_STICK_SOURCE, IDC_CSTICK_SOURCE, IDC_TRIGGER_SOURCE,
		};

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
	
		wxCheckBox *m_CheckS2C[4],
			*m_CheckRumble[4];

		wxButton *m_OK, *m_Cancel, *ClickedButton,
			*m_Button_Analog[IDB_TRIGGER_R - IDB_ANALOG_LEFT_X + 1][4],
			*m_Button_GC[IDB_SHDR_SEMI_R - IDB_BTN_A + 1][4];

		wxComboBox *m_Joyname[4],
			*m_ComboDeadZoneLeft[4],
			*m_ComboDeadZoneRight[4],
			*m_ComboDiagonal[4],
			*m_RumbleStrength[4],
			*m_TriggerType[4],
			*m_Combo_StickSrc[4],
			*m_Combo_CStickSrc[4],
			*m_Combo_TriggerSrc[4];

		wxGridBagSizer *m_sGridStickLeft[4],
			*m_sGridStickRight[4],
			*m_sGridTrigger[4];

		wxBoxSizer  *m_MainSizer,
			*m_sMain[4],
			*m_sDeadZoneHoriz[4],
			*m_sDeadZone[4],
			*m_sDiagonal[4],
			*m_sSquare2Circle[4],
			*m_sS2CDeadZone[4],
			*m_sRumbleStrength[4],
			*m_sRumble[4],
			*m_sHorizJoypad[4],
			*m_sHorizStatus[4],
			*m_Sizer_Analog[IDB_TRIGGER_R - IDB_ANALOG_LEFT_X + 1][4],
			*m_sAnalogLeft[4],
			*m_sAnalogMiddle[4],
			*m_sAnalogRight[4],
			*m_sHorizAnalog[4],
			*m_sStickSrc[4],
			*m_sCStickSrc[4],
			*m_sTriggerSrc[4],
			*m_Sizer_Pad[IDB_SHDR_SEMI_R - IDB_BTN_A + 1][4],
			*m_sHorizMapping[4];

		wxStaticBoxSizer *m_gJoyPad[4],
			*m_gStickLeft[4],
			*m_gStickRight[4],
			*m_gTriggers[4],
			*m_gAnalog[4],
			*m_gButton[4],
			*m_gDPad[4],
			*m_gStick[4],
			*m_gCStick[4],
			*m_gTrigger[4];

		wxStaticText *m_ComboDeadZoneLabel[4],
			*m_DiagonalLabel[4],
			*m_RumbleStrengthLabel[4],
			*m_tStatusLeftIn[4], *m_tStatusLeftOut[4],
			*m_tStatusRightIn[4], *m_tStatusRightOut[4],
			*m_TriggerL[4], *m_TriggerR[4],
			*m_TriggerStatusL[4], *m_TriggerStatusR[4],
			*m_tTriggerSource[4],
			*m_Text_Analog[IDB_TRIGGER_R - IDB_ANALOG_LEFT_X + 1][4],
			*m_Text_Pad[IDB_SHDR_SEMI_R - IDB_BTN_A + 1][4],
			*m_Text_StickSrc[5],
			*m_Text_CStickSrc[5],
			*m_Text_TriggerSrc[5];

		wxStaticBitmap *m_bmpDotLeftIn[4],
			*m_bmpDotLeftOut[4],
			*m_bmpDotRightIn[4],
			*m_bmpDotRightOut[4],
			*m_bmpDeadZoneLeftIn[4],
			*m_bmpDeadZoneRightIn[4];

		bool m_ControlsCreated;
		int m_Page, BoxW, BoxH;
		int GetButtonWaitingID, GetButtonWaitingTimer, g_Pressed;
		wxString OldLabel;

	#if wxUSE_TIMER
		wxTimer *m_UpdatePadTimer, *m_ButtonMappingTimer;
		void UpdatePadInfo(wxTimerEvent& WXUNUSED(event));
		void OnButtonTimer(wxTimerEvent& WXUNUSED(event)) { DoGetButtons(GetButtonWaitingID); }
	#endif

		wxBitmap CreateBitmap();
		wxBitmap CreateBitmapDot();
		wxBitmap CreateBitmapDeadZone(int Radius);
		wxBitmap CreateBitmapClear();

		void NotebookPageChanged(wxNotebookEvent& event);
		void OnClose(wxCloseEvent& event);
		void OnCloseClick(wxCommandEvent& event);
		void OnKeyDown(wxKeyEvent& event);
		void OnButtonClick(wxCommandEvent& event);
		void OnAxisClick(wxCommandEvent& event);
		void ChangeSettings(wxCommandEvent& event);
		void SaveButtonMapping(int Id, int Key);
		void UpdateGUI();
		void CreateGUIControls();
 
		void Convert2Box(int &x);
		void DoChangeDeadZone();
		void ToBlank(bool ToBlank, int Id);

		void DoGetButtons(int _GetId);
		void SetButtonText(int id, const wxString &str);
		wxString GetButtonText(int id);
};

#endif // __GCPAD_CONFIGBOX_h__
