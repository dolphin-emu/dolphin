// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>
#include <cstdio>

#include <wx/chartype.h>
#include <wx/defs.h>
#include <wx/gdicmn.h>
#include <wx/listbase.h>
#include <wx/listctrl.h>
#include <wx/string.h>
#include <wx/windowid.h>

#include "Common/BreakPoints.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/BreakpointView.h"
#include "DolphinWX/Debugger/DebuggerUIUtil.h"

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

	InsertColumn(0, _("Active"));
	InsertColumn(1, _("Type"));
	InsertColumn(2, _("Function"));
	InsertColumn(3, _("Address"));
	InsertColumn(4, _("Flags"));

	const BreakPoints::TBreakPoints& rBreakPoints = PowerPC::breakpoints.GetBreakPoints();
	for (const auto& rBP : rBreakPoints)
	{
		if (!rBP.bTemporary)
		{
			wxString breakpoint_enabled_str = StrToWxStr(rBP.bOn ? "on" : " ");
			int item = InsertItem(0, breakpoint_enabled_str);
			SetItem(item, 1, StrToWxStr("BP"));

			Symbol *symbol = g_symbolDB.GetSymbolFromAddr(rBP.iAddress);
			if (symbol)
			{
				wxString symbol_description = StrToWxStr(g_symbolDB.GetDescription(rBP.iAddress));
				SetItem(item, 2, symbol_description);
			}

			std::string address = StringFromFormat("%08x", rBP.iAddress);
			SetItem(item, 3, StrToWxStr(address));

			SetItemData(item, rBP.iAddress);
		}
	}

	const MemChecks::TMemChecks& rMemChecks = PowerPC::memchecks.GetMemChecks();
	for (const auto& rMemCheck : rMemChecks)
	{
		wxString memcheck_on_str = StrToWxStr((rMemCheck.Break || rMemCheck.Log) ? "on" : " ");
		int item = InsertItem(0, memcheck_on_str);
		SetItem(item, 1, StrToWxStr("MC"));

		Symbol *symbol = g_symbolDB.GetSymbolFromAddr(rMemCheck.StartAddress);
		if (symbol)
		{
			wxString memcheck_start_addr = StrToWxStr(g_symbolDB.GetDescription(rMemCheck.StartAddress));
			SetItem(item, 2, memcheck_start_addr);
		}

		std::string address_range_str = StringFromFormat("%08x to %08x", rMemCheck.StartAddress, rMemCheck.EndAddress);
		SetItem(item, 3, StrToWxStr(address_range_str));

		std::string mode;
		if (rMemCheck.OnRead)
			mode += 'r';
		if (rMemCheck.OnWrite)
			mode += 'w';

		SetItem(item, 4, StrToWxStr(mode));

		SetItemData(item, rMemCheck.StartAddress);
	}

	Refresh();
}

void CBreakPointView::DeleteCurrentSelection()
{
	int item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (item >= 0)
	{
		u32 Address = (u32)GetItemData(item);
		PowerPC::breakpoints.Remove(Address);
		PowerPC::memchecks.Remove(Address);
		Update();
	}
}
