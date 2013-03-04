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

#include <wx/wx.h>

#include "BreakpointView.h"
#include "DebuggerUIUtil.h"
#include "Debugger/Debugger_SymbolMap.h"
#include "PowerPC/PPCSymbolDB.h"
#include "PowerPC/PowerPC.h"
#include "HW/Memmap.h"
#include "../WxUtils.h"

CBreakPointView::CBreakPointView(wxWindow* parent, const wxWindowID id)
	: wxListCtrl(parent, id, wxDefaultPosition, wxDefaultSize,
			wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL | wxLC_SORT_ASCENDING)
{
	SetFont(DebuggerFont);
	Refresh();
}

void CBreakPointView::Update()
{
	ClearAll();

	InsertColumn(0, wxT("Active"));
	InsertColumn(1, wxT("Type"));
	InsertColumn(2, wxT("Function"));
    InsertColumn(3, wxT("Address"));
    InsertColumn(4, wxT("Flags"));

    char szBuffer[64];
	const BreakPoints::TBreakPoints& rBreakPoints = PowerPC::breakpoints.GetBreakPoints();
	for (size_t i = 0; i < rBreakPoints.size(); i++)
	{
		const TBreakPoint& rBP = rBreakPoints[i];
		if (!rBP.bTemporary)
		{
			wxString temp;
			temp = StrToWxStr(rBP.bOn ? "on" : " ");
			int Item = InsertItem(0, temp);
			temp = StrToWxStr("BP");
			SetItem(Item, 1, temp);
			
			Symbol *symbol = g_symbolDB.GetSymbolFromAddr(rBP.iAddress);
			if (symbol)
			{
				temp = StrToWxStr(g_symbolDB.GetDescription(rBP.iAddress));
				SetItem(Item, 2, temp);
			}
			
            sprintf(szBuffer, "%08x", rBP.iAddress);
            temp = StrToWxStr(szBuffer);
			SetItem(Item, 3, temp);

            SetItemData(Item, rBP.iAddress);
		}
	}

	const MemChecks::TMemChecks& rMemChecks = PowerPC::memchecks.GetMemChecks();
	for (size_t i = 0; i < rMemChecks.size(); i++)
	{
		const TMemCheck& rMemCheck = rMemChecks[i];

		wxString temp;
		temp = StrToWxStr((rMemCheck.Break || rMemCheck.Log) ? "on" : " ");
		int Item = InsertItem(0, temp);
		temp = StrToWxStr("MC");
		SetItem(Item, 1, temp);

		Symbol *symbol = g_symbolDB.GetSymbolFromAddr(rMemCheck.StartAddress);
		if (symbol)
		{
			temp = StrToWxStr(g_symbolDB.GetDescription(rMemCheck.StartAddress));
			SetItem(Item, 2, temp);
		}

		sprintf(szBuffer, "%08x to %08x", rMemCheck.StartAddress, rMemCheck.EndAddress);
		temp = StrToWxStr(szBuffer);
		SetItem(Item, 3, temp);

		size_t c = 0;
		if (rMemCheck.OnRead) szBuffer[c++] = 'r';
		if (rMemCheck.OnWrite) szBuffer[c++] = 'w';
		szBuffer[c] = 0x00;
		temp = StrToWxStr(szBuffer);
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
        PowerPC::breakpoints.Remove(Address);
        PowerPC::memchecks.Remove(Address);
		Update();
    }
}
