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

#ifndef JITWINDOW_H_
#define JITWINDOW_H_

#include <vector>

#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/listctrl.h>
#include <wx/listbox.h>
#include "Debugger.h"
#include "MemoryView.h"
#include "Thread.h"
#include "IniFile.h"

#include "CoreParameter.h"


class JitBlockList : public wxListCtrl
{
	std::vector<int> block_ranking;
public:
	JitBlockList(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style);
	void Init();
	void Update();
};


class CJitWindow : public wxFrame
{
public:
	CJitWindow(wxWindow* parent,
		wxWindowID id = wxID_ANY,
		const wxString& title = _T("JIT block viewer"),
		const wxPoint& pos = wxPoint(950, 100),
		const wxSize& size = wxSize(400, 500),
		long style = wxDEFAULT_FRAME_STYLE | wxCLIP_CHILDREN | wxNO_FULL_REPAINT_ON_RESIZE);

    ~CJitWindow();

	void Save(IniFile& _IniFile) const;
	void Load(IniFile& _IniFile);

	static void ViewAddr(u32 em_address);
	void Update();

private:
	void OnRefresh(wxCommandEvent& /*event*/);
	void Compare(u32 em_address);

	JitBlockList* block_list;
	wxButton* button_refresh;
	wxTextCtrl* ppc_box;
	wxTextCtrl* x86_box;
	wxListBox* top_instructions;

	DECLARE_EVENT_TABLE()

	void OnSymbolListChange(wxCommandEvent& event);
	void OnCallstackListChange(wxCommandEvent& event);
	void OnAddrBoxChange(wxCommandEvent& event);
	void OnHostMessage(wxCommandEvent& event);
};

#endif /*MEMORYWINDOW_*/
