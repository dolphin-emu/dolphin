// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <wx/bitmap.h>
#include <wx/dcmemory.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/string.h>
#include <wx/toplevel.h>
#include <wx/translation.h>
#include <wx/windowid.h>

#include "Common/Common.h"
#include "InputCommon/GCPadStatus.h"

class wxCheckBox;
class wxSlider;
class wxStaticBitmap;
class wxTextCtrl;
class wxWindow;

class TASInputDlg : public wxDialog
{
	public:
		TASInputDlg(wxWindow *parent,
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
		void GetValues(GCPadStatus* PadStatus, int controllerID);
		void SetTurbo(wxMouseEvent& event);
		void SetTurboFalse(wxMouseEvent& event);
		void ButtonTurbo();
		void GetKeyBoardInput(GCPadStatus* PadStatus);
		bool TextBoxHasFocus();
		void SetLandRTriggers();
		bool TASHasFocus();

		wxBitmap CreateStickBitmap(int x, int y);

	private:
		u8 mainX, mainY, cX, cY, lTrig, rTrig;

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

		wxSlider *wx_mainX_s, *wx_mainY_s, *wx_cX_s, *wx_cY_s, *wx_l_s, *wx_r_s;
		wxCheckBox *wx_up_button, *wx_down_button, *wx_left_button, *wx_right_button, *wx_a_button, *wx_b_button, *wx_x_button, *wx_y_button, *wx_l_button, *wx_r_button, *wx_z_button, *wx_start_button;
		wxTextCtrl *wx_mainX_t, *wx_mainY_t, *wx_cX_t, *wx_cY_t, *wx_l_t, *wx_r_t;
		wxMemoryDC dc_main,  dc_c;
		wxStaticBitmap* static_bitmap_main, *static_bitmap_c;
		wxBitmap bitmap;
		bool A_turbo,B_turbo, X_turbo, Y_turbo, Z_turbo, L_turbo, R_turbo, START_turbo,DL_turbo,DR_turbo,DD_turbo,DU_turbo;
		bool A_cont,B_cont, X_cont, Y_cont, Z_cont, L_cont, L_button_cont, R_cont, R_button_cont, START_cont,DL_cont,DR_cont,DD_cont,DU_cont,mstickx,msticky,cstickx,csticky;
		int xaxis,yaxis,c_xaxis,c_yaxis,update,update_axis;

		DECLARE_EVENT_TABLE();
};
