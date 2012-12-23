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

#ifndef _DSP_DEBUGGER_LLE_H
#define _DSP_DEBUGGER_LLE_H

// general things
#include <iostream>
#include <vector>
#include <list>
#include <map>
#include <algorithm>

#include <wx/wx.h>
#include <wx/frame.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/statbox.h>
#include <wx/sizer.h>
#include <wx/listctrl.h>
#include <wx/statline.h>
#include <wx/aui/aui.h>

#include "DSP/disassemble.h"
#include "DSP/DSPInterpreter.h"
#include "DSP/DSPMemoryMap.h"
#include "HW/DSPLLE/DSPDebugInterface.h"

class DSPRegisterView;
class CCodeView;
class CMemoryView;

class DSPDebuggerLLE : public wxPanel
{
public:
	DSPDebuggerLLE(wxWindow *parent, wxWindowID id = wxID_ANY);
	virtual ~DSPDebuggerLLE();

	virtual void Update();

private:
	DECLARE_EVENT_TABLE();

	enum
	{
		ID_TOOLBAR = 1000,
		ID_RUNTOOL,
		ID_STEPTOOL,
		ID_SHOWPCTOOL,
		ID_ADDRBOX,
		ID_SYMBOLLIST,
		ID_DSP_REGS
	};

	DSPDebugInterface debug_interface;
	u64 m_CachedStepCounter;

	// GUI updaters
	void UpdateDisAsmListView();
	void UpdateRegisterFlags();
	void UpdateSymbolMap();
	void UpdateState();

	// GUI items
	wxAuiManager m_mgr;
	wxAuiToolBar* m_Toolbar;
	CCodeView* m_CodeView;
	CMemoryView* m_MemView;
	DSPRegisterView* m_Regs;
	wxListBox* m_SymbolList;
	wxAuiNotebook* m_MainNotebook;

	void OnClose(wxCloseEvent& event);
	void OnChangeState(wxCommandEvent& event);
	//void OnRightClick(wxListEvent& event);
	//void OnDoubleClick(wxListEvent& event);
	void OnAddrBoxChange(wxCommandEvent& event);
	void OnSymbolListChange(wxCommandEvent& event);

	bool JumpToAddress(u16 addr);

	void FocusOnPC();
	//void UnselectAll();
};

extern DSPDebuggerLLE* m_DebuggerFrame;

#endif //_DSP_DEBUGGER_LLE_H
