// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Debugger/BreakpointView.h"

#include <cstddef>
#include <cstdio>

#include <wx/listctrl.h>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Core/PowerPC/BreakPoints.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/Debugger/DebuggerUIUtil.h"
#include "DolphinWX/WxUtils.h"

CBreakPointView::CBreakPointView(wxWindow* parent, const wxWindowID id)
    : wxListCtrl(parent, id, wxDefaultPosition, wxDefaultSize,
                 wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL |
                     wxLC_SORT_ASCENDING)
{
  SetFont(DebuggerFont);
  Refresh();
}

void CBreakPointView::Repopulate()
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
    if (!rBP.is_temporary)
    {
      wxString breakpoint_enabled_str = StrToWxStr(rBP.is_enabled ? "on" : " ");
      int item = InsertItem(0, breakpoint_enabled_str);
      SetItem(item, 1, StrToWxStr("BP"));

      Symbol* symbol = g_symbolDB.GetSymbolFromAddr(rBP.address);
      if (symbol)
      {
        wxString symbol_description = StrToWxStr(g_symbolDB.GetDescription(rBP.address));
        SetItem(item, 2, symbol_description);
      }

      std::string address = StringFromFormat("%08x", rBP.address);
      SetItem(item, 3, StrToWxStr(address));

      SetItemData(item, rBP.address);
    }
  }

  const MemChecks::TMemChecks& rMemChecks = PowerPC::memchecks.GetMemChecks();
  for (const auto& rMemCheck : rMemChecks)
  {
    const wxString memcheck_on_str =
        StrToWxStr((rMemCheck.break_on_hit || rMemCheck.log_on_hit) ? "on" : " ");
    int item = InsertItem(0, memcheck_on_str);
    SetItem(item, 1, StrToWxStr("MBP"));

    Symbol* symbol = g_symbolDB.GetSymbolFromAddr(rMemCheck.start_address);
    if (symbol)
    {
      wxString memcheck_start_addr = StrToWxStr(g_symbolDB.GetDescription(rMemCheck.start_address));
      SetItem(item, 2, memcheck_start_addr);
    }

    std::string address_range_str =
        StringFromFormat("%08x to %08x", rMemCheck.start_address, rMemCheck.end_address);
    SetItem(item, 3, StrToWxStr(address_range_str));

    std::string mode;
    if (rMemCheck.is_break_on_read)
      mode += 'r';
    if (rMemCheck.is_break_on_write)
      mode += 'w';

    SetItem(item, 4, StrToWxStr(mode));

    SetItemData(item, rMemCheck.start_address);
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
    Repopulate();
  }
}
