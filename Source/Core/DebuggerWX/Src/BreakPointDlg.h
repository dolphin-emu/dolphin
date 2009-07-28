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

#ifndef __BREAKPOINTDLG_h__
#define __BREAKPOINTDLG_h__


#include <wx/wx.h>
#include <wx/dialog.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/statbox.h>

#undef BreakPointDlg_STYLE
#define BreakPointDlg_STYLE wxCAPTION | wxSYSTEM_MENU | wxDIALOG_NO_PARENT | wxCLOSE_BOX


class BreakPointDlg : public wxDialog
{
	private:
		DECLARE_EVENT_TABLE();
		
	public:
		BreakPointDlg(wxWindow *parent, wxWindowID id = 1, const wxString &title = wxT("BreakPoint"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = BreakPointDlg_STYLE);
		virtual ~BreakPointDlg();
	
	private:

		wxButton *m_pButtonOK;
		wxButton *m_pButtonCancel;
		wxTextCtrl *m_pEditAddress;

	private:

		enum
		{
			ID_WXSTATICTEXT1 = 1006,
			ID_OK = 1005,
			ID_CANCEL = 1004,
			ID_ADDRESS = 1003,
			ID_WXSTATICBOX1 = 1001,
		};
	
	private:

		void CreateGUIControls();
		void OnClose(wxCloseEvent& event);
		void OnCancel(wxCommandEvent& event);
		void OnOK(wxCommandEvent& event);
};

#endif
