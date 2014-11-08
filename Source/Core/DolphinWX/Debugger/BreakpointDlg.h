// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <wx/dialog.h>
#include <wx/event.h>

class CBreakPointWindow;
class wxTextCtrl;

class BreakPointDlg : public wxDialog
{
public:
	BreakPointDlg(CBreakPointWindow *_Parent);

private:
	CBreakPointWindow *Parent;
	wxTextCtrl *m_pEditAddress;

	void OnOK(wxCommandEvent& event);
};
