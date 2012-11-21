// Copyright (C) 2011 Dolphin Project.

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

#ifndef __TASINPUT_H__
#define __TASINPUT_H__

#include <wx/wx.h>

#include "Common.h"
#include "CoreParameter.h"
#include "../../InputCommon/Src/GCPadStatus.h"

class GCTASInputDlg : public wxDialog
{
	public:
		GCTASInputDlg(wxWindow *parent,
				wxWindowID id = 1,
				const wxString &title = _("TAS Input"),
				const wxPoint& pos = wxDefaultPosition,
				const wxSize& size = wxDefaultSize,
				long style = wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP);
    
    	void OnCloseWindow(wxCloseEvent& event);
		void UpdateFromSliders(wxCommandEvent& event);
		void UpdateFromText(wxCommandEvent& event);
		void OnMouseDownL(wxMouseEvent& event);
		void OnMouseUpR(wxMouseEvent& event);
		void ResetValues();
		void GetValues(SPADStatus *PadStatus, int controllerID);
		void SetTurbo(wxMouseEvent& event);
		void SetTurboFalse(wxMouseEvent& event);
        void SetTurboState(wxCheckBox *CheckBox, bool *TurboOn);
		void ButtonTurbo();
		void GetKeyBoardInput(SPADStatus *PadStatus);
        bool TextBoxHasFocus();
        void SetLandRTriggers();
		bool TASInputDlgHasFocus();
        wxCheckBox* CreateCheckBoxAndListerners(int id, std::string name);

		wxBitmap CreateStickBitmap(int x, int y);
      
	private:
		int lTrig, rTrig;

		enum
		{
			ID_MAIN_X_SLIDER = 1000,
			ID_MAIN_X_TEXT,
			ID_MAIN_Y_SLIDER,
			ID_MAIN_Y_TEXT,
			ID_C_X_SLIDER,
			ID_C_X_TEXT,
			ID_C_Y_SLIDER,
			ID_C_Y_TEXT,
			ID_L_SLIDER,
			ID_L_TEXT,
			ID_R_SLIDER,
			ID_R_TEXT,
			ID_CLOSE,
			ID_UP,
			ID_DOWN,
			ID_LEFT,
			ID_RIGHT,
			ID_A,
			ID_B,
			ID_X,
			ID_Y,
			ID_Z,
			ID_L,
			ID_R,
			ID_START,
			ID_MAIN_STICK,
			ID_C_STICK,
		};
    
        struct Button
        {
            wxCheckBox *Checkbox;
            bool SetByKeyboard = false;
            bool TurboOn = false;
        };
    
        struct Stick
        {
            wxStaticBitmap *bitmap;
            wxTextCtrl *xText;
            wxTextCtrl *yText;
            wxSlider *xSlider;
            wxSlider *ySlider;
            int x = 128;
            int y = 128;
        };
    
        void SetStickValue(bool *ActivatedByKeyboard, int *AmountPressed, wxTextCtrl *Textbox, int CurrentValue);
        void SetButtonValue(Button *button,bool CurrentState);
    
		wxSlider *wx_cY_s, *wx_l_s, *wx_r_s;
		wxTextCtrl *wx_l_t, *wx_r_t;
		bool msticky,cstickx,csticky,mstickx;
        Button A, B, X, Y, Z, L, R, START, dpad_up, dpad_down, dpad_left, dpad_right;
        Stick MainStick,CStick;
    
		DECLARE_EVENT_TABLE();
};

#endif
