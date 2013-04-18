// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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

		void OnOK(wxCommandEvent& event);

		DECLARE_EVENT_TABLE();
};

#endif
