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

#ifndef MEMORYWINDOW_H_
#define MEMORYWINDOW_H_

#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/listbox.h>
#include "Debugger.h"
#include "MemoryView.h"
#include "Thread.h"
#include "StringUtil.h"

#include "CoreParameter.h"

class CRegisterWindow;
class CBreakPointWindow;

class CMemoryWindow
	: public wxFrame
{
	public:

		CMemoryWindow(wxWindow* parent,
			wxWindowID id = wxID_ANY,
			const wxString& title = _T("Dolphin-Memory"),
		const wxPoint& pos = wxPoint(950, 100),
		const wxSize& size = wxSize(400, 500),
		long style = wxDEFAULT_FRAME_STYLE | wxCLIP_CHILDREN | wxNO_FULL_REPAINT_ON_RESIZE);

        ~CMemoryWindow();

		void Save(IniFile& _IniFile) const;
		void Load(IniFile& _IniFile);

		void Update();
		void NotifyMapLoaded();

        void JumpToAddress(u32 _Address);

	private:
		CMemoryView* memview;
		wxListBox* symbols;

		wxButton* buttonGo;
		wxTextCtrl* addrbox;
		wxTextCtrl* valbox;

		DECLARE_EVENT_TABLE()

		void OnSymbolListChange(wxCommandEvent& event);
		void OnCallstackListChange(wxCommandEvent& event);
		void OnAddrBoxChange(wxCommandEvent& event);
		void OnHostMessage(wxCommandEvent& event);
		void SetMemoryValue(wxCommandEvent& event);
		void OnDumpMemory(wxCommandEvent& event);
};

#endif /*MEMORYWINDOW_*/
