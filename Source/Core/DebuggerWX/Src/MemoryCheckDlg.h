// Copyright (C) 2003-2008 Dolphin Project.

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

#ifndef __MEMORYCHECKDLG_h__
#define __MEMORYCHECKDLG_h__

#include <wx/wx.h>
#include <wx/dialog.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/statbox.h>

#undef MemoryCheckDlg_STYLE
#define MemoryCheckDlg_STYLE wxCAPTION | wxSYSTEM_MENU | wxSTAY_ON_TOP | wxDIALOG_NO_PARENT | wxCLOSE_BOX

class MemoryCheckDlg : public wxDialog
{
	private:
		DECLARE_EVENT_TABLE();
		
	public:
		MemoryCheckDlg(wxWindow *parent, wxWindowID id = 1, const wxString &title = wxT("Memory Check"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = MemoryCheckDlg_STYLE);
		virtual ~MemoryCheckDlg();
	
	private:

		wxButton* m_pButtonCancel;
		wxButton* m_pButtonOK;
		wxCheckBox* m_pReadFlag;
		wxCheckBox* m_pWriteFlag;
		wxTextCtrl* m_pEditEndAddress;
		wxTextCtrl* m_pEditStartAddress;

	private:

		enum
		{
			ID_CANCEL = 1016,
			ID_OK = 1015,
			ID_READ_FLAG = 1014,
			ID_WRITE_FLAG = 1013,
			ID_WXSTATICBOX2 = 1012,
			ID_EDIT_END_ADDRESS = 1011,
			ID_WXSTATICTEXT2 = 1010,
			ID_WXSTATICTEXT1 = 1009,
			ID_EDIT_START_ADDR = 1008,
			ID_WXSTATICBOX1 = 1007,
		};
	
	private:
		void OnClose(wxCloseEvent& event);
		void OnOK(wxCommandEvent& event);
		void OnCancel(wxCommandEvent& event);
		void CreateGUIControls();
};

#endif
