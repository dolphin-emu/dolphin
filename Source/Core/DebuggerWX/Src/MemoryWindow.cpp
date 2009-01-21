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

#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/listctrl.h>
#include <wx/thread.h>
#include <wx/listctrl.h>
#include "MemoryWindow.h"
#include "HW/CPU.h"
#include "PowerPC/PowerPC.h"
#include "Host.h"

#include "Debugger/PPCDebugInterface.h"
#include "PowerPC/SymbolDB.h"

#include "Core.h"
#include "LogManager.h"

#include "HW/Memmap.h"

// ugly that this lib included code from the main
#include "../../DolphinWX/Src/Globals.h"


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
	IDM_BREAKPOINTWINDOW,
	IDM_VALBOX,
	IDM_SETVALBUTTON,
	IDM_DUMP_MEMORY,

};

BEGIN_EVENT_TABLE(CMemoryWindow, wxFrame)
    EVT_TEXT(IDM_ADDRBOX,           CMemoryWindow::OnAddrBoxChange)
    EVT_LISTBOX(IDM_SYMBOLLIST,     CMemoryWindow::OnSymbolListChange)
    EVT_HOST_COMMAND(wxID_ANY,      CMemoryWindow::OnHostMessage)
	EVT_BUTTON(IDM_SETVALBUTTON,    CMemoryWindow::SetMemoryValue)
	EVT_BUTTON(IDM_DUMP_MEMORY,     CMemoryWindow::OnDumpMemory)
END_EVENT_TABLE()


CMemoryWindow::CMemoryWindow(wxWindow* parent, wxWindowID id,
		const wxString& title, const wxPoint& pos, const wxSize& size, long style)
	: wxFrame(parent, id, title, pos, size, style)
{    
	wxBoxSizer* sizerBig   = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sizerRight = new wxBoxSizer(wxVERTICAL);
	// didn't see anything usefull in the left part
	//wxBoxSizer* sizerLeft  = new wxBoxSizer(wxVERTICAL);

	DebugInterface* di = new PPCDebugInterface();

	//sizerLeft->Add(symbols = new wxListBox(this, IDM_SYMBOLLIST, wxDefaultPosition, wxSize(20, 100), 0, NULL, wxLB_SORT), 1, wxEXPAND);
	memview = new CMemoryView(di, this, wxID_ANY);
	//sizerBig->Add(sizerLeft, 1, wxEXPAND);
	sizerBig->Add(memview, 20, wxEXPAND);
	sizerBig->Add(sizerRight, 0, wxEXPAND | wxALL, 3);
	sizerRight->Add(buttonGo = new wxButton(this, IDM_DEBUG_GO, _T("&Go")));
	sizerRight->Add(addrbox = new wxTextCtrl(this, IDM_ADDRBOX, _T("")));
	sizerRight->Add(new wxButton(this, IDM_SETPC, _T("S&et PC")));
	sizerRight->Add(valbox = new wxTextCtrl(this, IDM_VALBOX, _T("")));
	sizerRight->Add(new wxButton(this, IDM_SETVALBUTTON, _T("Set &Value")));
	
	sizerRight->AddSpacer(5);
	sizerRight->Add(new wxButton(this, IDM_DUMP_MEMORY, _T("&Dump Memory")));

	SetSizer(sizerBig);

	//sizerLeft->SetSizeHints(this);
	//sizerLeft->Fit(this);
	sizerRight->SetSizeHints(this);
	sizerRight->Fit(this);
	sizerBig->SetSizeHints(this);
	sizerBig->Fit(this);
}


CMemoryWindow::~CMemoryWindow()
{
}


void CMemoryWindow::Save(IniFile& _IniFile) const
{
	// Prevent these bad values that can happen after a crash or hanging
	if(GetPosition().x != -32000 && GetPosition().y != -32000)
	{
		_IniFile.Set("MemoryWindow", "x", GetPosition().x);
		_IniFile.Set("MemoryWindow", "y", GetPosition().y);
		_IniFile.Set("MemoryWindow", "w", GetSize().GetWidth());
		_IniFile.Set("MemoryWindow", "h", GetSize().GetHeight());
	}
}


void CMemoryWindow::Load(IniFile& _IniFile)
{
	int x,y,w,h;
	_IniFile.Get("MemoryWindow", "x", &x, GetPosition().x);
	_IniFile.Get("MemoryWindow", "y", &y, GetPosition().y);
	_IniFile.Get("MemoryWindow", "w", &w, GetSize().GetWidth());
	_IniFile.Get("MemoryWindow", "h", &h, GetSize().GetHeight());
	SetSize(x, y, w, h);
}


void CMemoryWindow::JumpToAddress(u32 _Address)
{
    memview->Center(_Address);
}


void CMemoryWindow::SetMemoryValue(wxCommandEvent& event)
{
	std::string str_addr = std::string(addrbox->GetValue().mb_str());
	std::string str_val = std::string(valbox->GetValue().mb_str());
	u32 addr;
	u32 val;

	if (!TryParseUInt(std::string("0x") + str_addr, &addr)) {
            PanicAlert("Invalid Address: %s", str_addr.c_str());
            return;
	}

	if (!TryParseUInt(std::string("0x") + str_val, &val)) {
            PanicAlert("Invalid Value: %s", str_val.c_str());
            return;
	}

	Memory::Write_U32(val, addr);
	memview->Refresh();
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
	/*
#ifdef _WIN32
	const FunctionDB::XFuncMap &syms = g_symbolDB.Symbols();
	for (FuntionDB::XFuncMap::iterator iter = syms.begin(); iter != syms.end(); ++iter)
	{
		int idx = symbols->Append(iter->second.name.c_str());
		symbols->SetClientData(idx, (void*)&iter->second);
	}

	//
#endif
*/
	symbols->Show(true);
	Update();
}

void CMemoryWindow::OnSymbolListChange(wxCommandEvent& event)
{
	int index = symbols->GetSelection();
	if (index >= 0) {
		Symbol* pSymbol = static_cast<Symbol *>(symbols->GetClientData(index));
		if (pSymbol != NULL)
		{
			memview->Center(pSymbol->address);
		}
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

// this is a simple main 1Tsram dump,
// so we can view memory in a tile/hex viewer for data analysis
void CMemoryWindow::OnDumpMemory( wxCommandEvent& event )
{
	FILE* pDumpFile = fopen(FULL_MEMORY_DUMP_DIR, "wb");
	if (pDumpFile)
	{
		if (Memory::m_pRAM)
		{
			fwrite(Memory::m_pRAM, Memory::REALRAM_SIZE, 1, pDumpFile);
		}
		fclose(pDumpFile);
	}
}

