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
#include "JitWindow.h"
#include "HW/CPU.h"
#include "PowerPC/PowerPC.h"
#include "PowerPC/Jit64/Jit.h"
#include "PowerPC/Jit64/JitCache.h"
#include "Host.h"
#include "disasm.h"

#include "Debugger/PPCDebugInterface.h"
#include "Debugger/Debugger_SymbolMap.h"

#include "Core.h"
#include "StringUtil.h"
#include "LogManager.h"

// ugly that this lib included code from the main
#include "../../DolphinWX/src/Globals.h"

// UGLY
namespace {
CJitWindow *the_jit_window;
}

enum
{
	IDM_REFRESH_LIST = 33350,
	IDM_PPC_BOX,
	IDM_X86_BOX,
	IDM_NEXT,
	IDM_PREV,
	IDM_BLOCKLIST,
};

BEGIN_EVENT_TABLE(CJitWindow, wxFrame)
//    EVT_TEXT(IDM_ADDRBOX,           CJitWindow::OnAddrBoxChange)
  //  EVT_LISTBOX(IDM_SYMBOLLIST,     CJitWindow::OnSymbolListChange)
    //EVT_HOST_COMMAND(wxID_ANY,      CJitWindow::OnHostMessage)
	EVT_BUTTON(IDM_REFRESH_LIST, CJitWindow::OnRefresh)
END_EVENT_TABLE()


CJitWindow::CJitWindow(wxWindow* parent, wxWindowID id,
		const wxString& title, const wxPoint& pos, const wxSize& size, long style)
	: wxFrame(parent, id, title, pos, size, style)
{    
	the_jit_window = this;
	wxBoxSizer* sizerBig   = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* sizerSplit = new wxBoxSizer(wxHORIZONTAL);
	sizerSplit->Add(ppc_box = new wxTextCtrl(this, IDM_PPC_BOX, _T("(ppc)"), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE), 1, wxEXPAND);
	sizerSplit->Add(x86_box = new wxTextCtrl(this, IDM_X86_BOX, _T("(x86)"), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE), 1, wxEXPAND);
	sizerBig->Add(block_list = new JitBlockList(this, IDM_BLOCKLIST,
				wxDefaultPosition, wxSize(100, 140),
				wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL | wxLC_SORT_ASCENDING), 0, wxEXPAND);
	sizerBig->Add(sizerSplit, 2, wxEXPAND);
//	sizerBig->Add(memview, 5, wxEXPAND);
//	sizerBig->Add(sizerRight, 0, wxEXPAND | wxALL, 3);
	sizerBig->Add(button_refresh = new wxButton(this, IDM_REFRESH_LIST, _T("&Refresh")));
//	sizerRight->Add(addrbox = new wxTextCtrl(this, IDM_ADDRBOX, _T("")));
//	sizerRight->Add(new wxButton(this, IDM_SETPC, _T("S&et PC")));

	SetSizer(sizerBig);

	sizerSplit->SetSizeHints(this);
	sizerSplit->Fit(this);
	sizerBig->SetSizeHints(this);
	sizerBig->Fit(this);
}


CJitWindow::~CJitWindow()
{
}


void CJitWindow::Save(IniFile& _IniFile) const
{
	_IniFile.Set("JitWindow", "x", GetPosition().x);
	_IniFile.Set("JitWindow", "y", GetPosition().y);
	_IniFile.Set("JitWindow", "w", GetSize().GetWidth());
	_IniFile.Set("JitWindow", "h", GetSize().GetHeight());
}


void CJitWindow::Load(IniFile& _IniFile)
{
	int x,y,w,h;
	_IniFile.Get("JitWindow", "x", &x, GetPosition().x);
	_IniFile.Get("JitWindow", "y", &y, GetPosition().y);
	_IniFile.Get("JitWindow", "w", &w, GetSize().GetWidth());
	_IniFile.Get("JitWindow", "h", &h, GetSize().GetHeight());
	SetSize(x, y, w, h);
}

void CJitWindow::OnRefresh(wxCommandEvent& /*event*/) {
	block_list->Update();
}

void CJitWindow::ViewAddr(u32 em_address)
{
	the_jit_window->Compare(em_address);
}

void CJitWindow::Compare(u32 em_address)
{
	u8 *xDis = new u8[65536];
	memset(xDis, 0, 65536);

	disassembler x64disasm;
	x64disasm.set_syntax_intel();

	int block_num = Jit64::GetBlockNumberFromAddress(em_address);
	if (block_num < 0)
	{
		ppc_box->SetValue(wxString::FromAscii(StringFromFormat("(non-code address: %08x)", em_address).c_str()));
		x86_box->SetValue(wxString::FromAscii(StringFromFormat("(no translation)").c_str()));
		return;
	}
	Jit64::JitBlock *block = Jit64::GetBlock(block_num);
	const u8 *code = (const u8 *)Jit64::GetCompiledCodeFromBlock(block_num);
	u64 disasmPtr = (u64)code;
	int size = block->codeSize;
	const u8 *end = code + size;
	char *sptr = (char*)xDis;

	while ((u8*)disasmPtr < end)
	{
		disasmPtr += x64disasm.disasm64(disasmPtr, disasmPtr, (u8*)disasmPtr, sptr);
		sptr += strlen(sptr);
		*sptr++ = 13;
		*sptr++ = 10;
	}
	x86_box->SetValue(wxString::FromAscii((char*)xDis));
	delete [] xDis;
}

void CJitWindow::Update()
{

}

void CJitWindow::OnHostMessage(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	    case IDM_NOTIFYMAPLOADED:
		    //NotifyMapLoaded();
		    break;
	}
}



// JitBlockList
//================

enum {
	COLUMN_ADDRESS,
	COLUMN_PPCSIZE,
	COLUMN_X86SIZE,
	COLUMN_NAME,
	COLUMN_FLAGS,
	COLUMN_NUMEXEC,
	COLUMN_COST,  // (estimated as x86size * numexec)
};

JitBlockList::JitBlockList(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
	: wxListCtrl(parent, id, pos, size, style)                                                                                                                 // | wxLC_VIRTUAL)
{
	Init();		
}

void JitBlockList::Init()
{
	InsertColumn(COLUMN_ADDRESS, _T("Address"));
	InsertColumn(COLUMN_PPCSIZE, _T("PPC Size"));
	InsertColumn(COLUMN_X86SIZE, _T("x86 Size"));
	InsertColumn(COLUMN_NAME, _T("Symbol"));
	InsertColumn(COLUMN_FLAGS, _T("Flags"));
	InsertColumn(COLUMN_NUMEXEC, _T("NumExec"));
	InsertColumn(COLUMN_COST, _T("Cost"));
}

void JitBlockList::Update()
{
}
