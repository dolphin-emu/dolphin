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

#ifndef __MEMORYCHECKDLG_h__
#define __MEMORYCHECKDLG_h__

#include <wx/wx.h>

class CBreakPointWindow;

class MemoryCheckDlg : public wxDialog
{
	public:
		MemoryCheckDlg(CBreakPointWindow *parent);
	
	private:
		CBreakPointWindow *m_parent;
		wxCheckBox* m_pReadFlag;
		wxCheckBox* m_pWriteFlag;
		wxCheckBox* m_log_flag;
		wxCheckBox* m_break_flag;
		wxTextCtrl* m_pEditEndAddress;
		wxTextCtrl* m_pEditStartAddress;

		void OnClose(wxCloseEvent& WXUNUSED(event));
		void OnOK(wxCommandEvent& WXUNUSED(event));
		void OnCancel(wxCommandEvent& WXUNUSED(event));

		DECLARE_EVENT_TABLE();
};

#endif
