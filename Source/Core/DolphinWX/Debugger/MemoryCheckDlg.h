// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/dialog.h>

class CBreakPointWindow;
class wxCheckBox;
class wxTextCtrl;

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
};
