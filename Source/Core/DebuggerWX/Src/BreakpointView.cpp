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

#include "Debugger.h"

#include "BreakpointView.h"
#include "Debugger/Debugger_SymbolMap.h"
#include "PowerPC/PPCSymbolDB.h"
#include "PowerPC/PowerPC.h"
#include "HW/Memmap.h"

#include <wx/mstream.h>

extern "C" {
#include "../resources/toolbar_add_breakpoint.c"
#include "../resources/toolbar_add_memorycheck.c"
#include "../resources/toolbar_delete.c"
}


CBreakPointView::CBreakPointView(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
	: wxListCtrl(parent, id, pos, size, style)
{
	SetFont(DebuggerFont);

	Refresh();
}


void CBreakPointView::Update()
{
	ClearAll();

	InsertColumn(0, wxT("Active"), wxLIST_FORMAT_LEFT, 50);
	InsertColumn(1, wxT("Type"), wxLIST_FORMAT_LEFT, 50);
	InsertColumn(2, wxT("Function"), wxLIST_FORMAT_CENTER, 200);
    InsertColumn(3, wxT("Address"), wxLIST_FORMAT_LEFT, 100);
    InsertColumn(4, wxT("Flags"), wxLIST_FORMAT_CENTER, 100);

    char szBuffer[64];
	const BreakPoints::TBreakPoints& rBreakPoints = PowerPC::breakpoints.GetBreakPoints();
	for (size_t i = 0; i < rBreakPoints.size(); i++)
	{
		const TBreakPoint& rBP = rBreakPoints[i];
		if (!rBP.bTemporary)
		{
			wxString temp;
			temp = wxString::FromAscii(rBP.bOn ? "on" : " ");
			int Item = InsertItem(0, temp);
			temp = wxString::FromAscii("BP");
			SetItem(Item, 1, temp);
			
			Symbol *symbol = g_symbolDB.GetSymbolFromAddr(rBP.iAddress);
			if (symbol)
			{
				temp = wxString::FromAscii(g_symbolDB.GetDescription(rBP.iAddress));
				SetItem(Item, 2, temp);
			}
			
            sprintf(szBuffer, "0x%08x", rBP.iAddress);
            temp = wxString::FromAscii(szBuffer);
			SetItem(Item, 3, temp);

            SetItemData(Item, rBP.iAddress);
		}
	}

	const MemChecks::TMemChecks& rMemChecks = PowerPC::memchecks.GetMemChecks();
	for (size_t i = 0; i < rMemChecks.size(); i++)
	{
		const TMemCheck& rMemCheck = rMemChecks[i];

		wxString temp;
		temp = wxString::FromAscii(rMemCheck.Break ? "on" : " ");
		int Item = InsertItem(0, temp);
		temp = wxString::FromAscii("MC");
		SetItem(Item, 1, temp);

		Symbol *symbol = g_symbolDB.GetSymbolFromAddr(rMemCheck.StartAddress);
		if (symbol)
		{
			temp = wxString::FromAscii(g_symbolDB.GetDescription(rMemCheck.StartAddress));
			SetItem(Item, 2, temp);
		}

		sprintf(szBuffer, "0x%08x to 0%08x", rMemCheck.StartAddress, rMemCheck.EndAddress);
		temp = wxString::FromAscii(szBuffer);
		SetItem(Item, 3, temp);

		size_t c = 0;
		if (rMemCheck.OnRead) szBuffer[c++] = 'r';
		if (rMemCheck.OnWrite) szBuffer[c++] = 'w';
		szBuffer[c] = 0x00;
		temp = wxString::FromAscii(szBuffer);
		SetItem(Item, 4, temp);

		SetItemData(Item, rMemCheck.StartAddress);
	}

	Refresh();
}

void CBreakPointView::DeleteCurrentSelection()
{
    int Item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (Item >= 0)
    {
        u32 Address = (u32)GetItemData(Item);
        PowerPC::breakpoints.DeleteByAddress(Address);
        PowerPC::memchecks.DeleteByAddress(Address);
		Update();
    }
}


CBreakPointBar::CBreakPointBar(CBreakPointWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
	: wxListCtrl((wxWindow*)parent, id, pos, size, style)
{
	BPWindow = parent;

	SetBackgroundColour(wxColour(0x555555));
	SetForegroundColour(wxColour(0xffffff));

	// load orignal size 48x48
	wxMemoryInputStream st1(toolbar_delete_png, sizeof(toolbar_delete_png));
	wxMemoryInputStream st2(toolbar_add_breakpoint_png, sizeof(toolbar_add_breakpoint_png));
	wxMemoryInputStream st3(toolbar_add_memcheck_png, sizeof(toolbar_add_memcheck_png));
	m_Bitmaps[Toolbar_Delete] = wxBitmap(wxImage(st1, wxBITMAP_TYPE_ANY, -1).Rescale(24,24), -1);
	m_Bitmaps[Toolbar_Add_BP] = wxBitmap(wxImage(st2, wxBITMAP_TYPE_ANY, -1).Rescale(24,24), -1);
	m_Bitmaps[Toolbar_Add_MC] = wxBitmap(wxImage(st3, wxBITMAP_TYPE_ANY, -1).Rescale(24,24), -1);

	m_imageListNormal = new wxImageList(24, 24);
	m_imageListNormal->Add(m_Bitmaps[Toolbar_Delete]);
	m_imageListNormal->Add(m_Bitmaps[Toolbar_Add_BP]);
	m_imageListNormal->Add(m_Bitmaps[Toolbar_Add_MC]);
	SetImageList(m_imageListNormal, wxIMAGE_LIST_NORMAL);

	PopulateBar();
}


void CBreakPointBar::PopulateBar()
{
	long Index = InsertItem(IDM_DELETE, _T("Delete"), 0);
	InsertItem(IDM_CLEAR, _T("Clear all"), 0);

	InsertItem(IDM_ADD_BREAKPOINT, _T("Add BP..."), 1);
	InsertItem(IDM_ADD_BREAKPOINTMANY, _T("Add BPs..."), 1);

    // just add memory breakpoints if you can use them
    if (Memory::AreMemoryBreakpointsActivated())
    {
		InsertItem(IDM_ADD_MEMORYCHECK, _T("Add MC..."), 2);
		InsertItem(IDM_ADD_MEMORYCHECKMANY, _T("Add MCs..."), 2);
    }
}
