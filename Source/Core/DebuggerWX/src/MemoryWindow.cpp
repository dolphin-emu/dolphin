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

#include "Debugger.h"

#include "IniFile.h"

#include "wx/button.h"
#include "wx/textctrl.h"
#include "wx/listctrl.h"
#include "wx/thread.h"
#include "wx/listctrl.h"
#include "MemoryWindow.h"
#include "HW/CPU.h"
#include "PowerPC/PowerPC.h"
#include "Host.h"

#include "Debugger/PPCDebugInterface.h"
#include "Debugger/Debugger_SymbolMap.h"

#include "Core.h"
#include "LogManager.h"

// ugly that this lib included code from the main
#include "../../DolphinWX/src/Globals.h"

class SymbolList
	: public wxListCtrl
{
	wxString OnGetItemText(long item, long column)
	{
		return(_T("hello"));
	}
};

enum
{
	IDM_DEBUG_GO = 350,
	IDM_STEP,
	IDM_STEPOVER,
	IDM_SKIP,
	IDM_SETPC,
	IDM_GOTOPC,
	IDM_ADDRBOX,
	IDM_CALLSTACKLIST,
	IDM_SYMBOLLIST,
	IDM_INTERPRETER,
	IDM_DUALCORE,
	IDM_LOGWINDOW,
	IDM_REGISTERWINDOW,
	IDM_BREAKPOINTWINDOW
};

BEGIN_EVENT_TABLE(CMemoryWindow, wxFrame)
    EVT_TEXT(IDM_ADDRBOX,           CMemoryWindow::OnAddrBoxChange)
    EVT_LISTBOX(IDM_SYMBOLLIST,     CMemoryWindow::OnSymbolListChange)
    EVT_HOST_COMMAND(wxID_ANY,      CMemoryWindow::OnHostMessage)
END_EVENT_TABLE()


CMemoryWindow::CMemoryWindow(wxWindow* parent, wxWindowID id,
		const wxString& title, const wxPoint& pos, const wxSize& size, long style)
	: wxFrame(parent, id, title, pos, size, style)
{    
	wxBoxSizer* sizerBig   = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sizerRight = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* sizerLeft  = new wxBoxSizer(wxVERTICAL);

	DebugInterface* di = new PPCDebugInterface();

	sizerLeft->Add(symbols = new wxListBox(this, IDM_SYMBOLLIST, wxDefaultPosition, wxSize(90, 100), 0, NULL, wxLB_SORT), 1, wxEXPAND);
	memview = new CMemoryView(di, this, wxID_ANY);
	sizerBig->Add(sizerLeft, 2, wxEXPAND);
	sizerBig->Add(memview, 5, wxEXPAND);
	sizerBig->Add(sizerRight, 0, wxEXPAND | wxALL, 3);
	sizerRight->Add(buttonGo = new wxButton(this, IDM_DEBUG_GO, _T("&Go")));
	sizerRight->Add(addrbox = new wxTextCtrl(this, IDM_ADDRBOX, _T("")));
	sizerRight->Add(new wxButton(this, IDM_SETPC, _T("S&et PC")));

	SetSizer(sizerBig);

	sizerLeft->SetSizeHints(this);
	sizerLeft->Fit(this);
	sizerRight->SetSizeHints(this);
	sizerRight->Fit(this);
	sizerBig->SetSizeHints(this);
	sizerBig->Fit(this);
}


CMemoryWindow::~CMemoryWindow()
{
}



void CMemoryWindow::JumpToAddress(u32 _Address)
{
    memview->Center(_Address);
}


void CMemoryWindow::OnAddrBoxChange(wxCommandEvent& event)
{
	wxString txt = addrbox->GetValue();
	if (txt.size() == 8)
	{
		u32 addr;
		sscanf(txt.mb_str(), "%08x", &addr);
		memview->Center(addr);
	}

	event.Skip(1);
}

void CMemoryWindow::Update()
{
	memview->Refresh();
	memview->Center(PC);
}

void CMemoryWindow::NotifyMapLoaded()
{
	symbols->Show(false); // hide it for faster filling
	symbols->Clear();
#ifdef _WIN32
	const Debugger::XVectorSymbol& syms = Debugger::AccessSymbols();

	for (int i = 0; i < (int)syms.size(); i++)
	{
		int idx = symbols->Append(syms[i].GetName().c_str());
		symbols->SetClientData(idx, (void*)&syms[i]);
	}

	//
#endif

	symbols->Show(true);
	Update();
}

void CMemoryWindow::OnSymbolListChange(wxCommandEvent& event)
{
	int index = symbols->GetSelection();
	Debugger::CSymbol* pSymbol = static_cast<Debugger::CSymbol*>(symbols->GetClientData(index));

	if (pSymbol != NULL)
	{
		memview->Center(pSymbol->vaddress);
	}
}

void CMemoryWindow::OnHostMessage(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	    case IDM_NOTIFYMAPLOADED:
		    NotifyMapLoaded();
		    break;
	}
}


