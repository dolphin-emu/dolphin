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
		void ResetValues();
		void GetValues(SPADStatus *PadStatus, int controllerID);

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
		};

		wxSlider *wx_mainX_s, *wx_mainY_s, *wx_cX_s, *wx_cY_s, *wx_l_s, *wx_r_s;
		wxTextCtrl *wx_mainX_t, *wx_mainY_t, *wx_cX_t, *wx_cY_t, *wx_l_t, *wx_r_t;

		DECLARE_EVENT_TABLE();
};
#endif

