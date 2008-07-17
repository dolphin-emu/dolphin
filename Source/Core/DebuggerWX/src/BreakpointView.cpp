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
#include "Common.h"

#include "BreakpointView.h"
#include "Debugger/Debugger_BreakPoints.h"
#include "Debugger/Debugger_SymbolMap.h"

BEGIN_EVENT_TABLE(CBreakPointView, wxListCtrl)

END_EVENT_TABLE()

CBreakPointView::CBreakPointView(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
	: wxListCtrl(parent, id, pos, size, style)
{
	SetFont(wxFont(9, wxSWISS, wxNORMAL, wxNORMAL, false, wxT("Segoe UI")));

	Refresh();
}


void
CBreakPointView::Update()
{
	ClearAll();

	InsertColumn(0, wxT("Active"), wxLIST_FORMAT_LEFT, 50);
	InsertColumn(1, wxT("Type"), wxLIST_FORMAT_LEFT, 50);
	InsertColumn(2, wxT("Function"), wxLIST_FORMAT_CENTER, 200);

	const CBreakPoints::TBreakPoints& rBreakPoints = CBreakPoints::GetBreakPoints();
	for (size_t i=0; i<rBreakPoints.size(); i++)
	{
		const TBreakPoint& rBP = rBreakPoints[i];
		if (!rBP.bTemporary)
		{
			int Item = InsertItem(0, rBP.bOn ? "on" : " ");
			SetItem(Item, 1, "BP");
			
			Debugger::XSymbolIndex index = Debugger::FindSymbol(rBP.iAddress);
			if (index > 0)
			{
				SetItem(Item, 2, Debugger::GetDescription(rBP.iAddress));
			}
			else
			{
				char szBuffer[32];
				sprintf(szBuffer, "0x%08x", rBP.iAddress);
				SetItem(Item, 2, szBuffer);
			}
		}
	}


	Refresh();
}

void CBreakPointView::DeleteCurrentSelection()
{

}