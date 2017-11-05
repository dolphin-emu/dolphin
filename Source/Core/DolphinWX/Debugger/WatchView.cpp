// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/colour.h>
#include <wx/grid.h>
#include <wx/menu.h>

#include "Common/GekkoDisassembler.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/Debugger/BreakpointWindow.h"
#include "DolphinWX/Debugger/CodeWindow.h"
#include "DolphinWX/Debugger/DebuggerUIUtil.h"
#include "DolphinWX/Debugger/MemoryWindow.h"
#include "DolphinWX/Debugger/RegisterView.h"
#include "DolphinWX/Debugger/WatchView.h"
#include "DolphinWX/Debugger/WatchWindow.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/WxUtils.h"

enum
{
  IDM_DELETEWATCH = 1,
  IDM_ADDMEMCHECK,
  IDM_VIEWMEMORY,
};

static std::string GetWatchName(int count)
{
  return PowerPC::watches.GetWatches().at(count - 1).name;
}

static u32 GetWatchAddr(int count)
{
  return PowerPC::watches.GetWatches().at(count - 1).address;
}

static u32 GetWatchValue(int count)
{
  return PowerPC::HostRead_U32(GetWatchAddr(count));
}

static void AddWatchAddr(int count, u32 value)
{
  PowerPC::watches.Add(value);
}

static void UpdateWatchAddr(int count, u32 value)
{
  PowerPC::watches.Update(count - 1, value);
}

static void SetWatchName(int count, const std::string& value)
{
  if ((count - 1) < (int)PowerPC::watches.GetWatches().size())
  {
    PowerPC::watches.UpdateName(count - 1, value);
  }
  else
  {
    PowerPC::watches.Add(0);
    PowerPC::watches.UpdateName(PowerPC::watches.GetWatches().size() - 1, value);
  }
}

static void SetWatchValue(int count, u32 value)
{
  PowerPC::HostWrite_U32(value, GetWatchAddr(count));
}

static wxString GetValueByRowCol(int row, int col)
{
  if (row == 0)
  {
    // Column Labels
    switch (col)
    {
    case 0:
      return _("Label");
    case 1:
      return _("Address");
    case 2:
      return _("Hexadecimal");
    case 3:
      // i18n: The base 10 numeral system. Not related to non-integer numbers
      return _("Decimal");
    case 4:
      // i18n: Data type used in computing
      return _("String");
    default:
      return wxEmptyString;
    }
  }
  else if (row <= (int)PowerPC::watches.GetWatches().size())
  {
    if (Core::IsRunning())
    {
      switch (col)
      {
      case 0:
        return wxString::Format("%s", GetWatchName(row));
      case 1:
        return wxString::Format("%08x", GetWatchAddr(row));
      case 2:
        return wxString::Format("%08x", GetWatchValue(row));
      case 3:
        return wxString::Format("%u", GetWatchValue(row));
      case 4:
      {
        u32 addr = GetWatchAddr(row);
        if (PowerPC::HostIsRAMAddress(addr))
          return PowerPC::HostGetString(addr, 32).c_str();
        else
          return wxEmptyString;
      }
      default:
        return wxEmptyString;
      }
    }
  }
  return wxEmptyString;
}

wxString CWatchTable::GetValue(int row, int col)
{
  return GetValueByRowCol(row, col);
}

void CWatchTable::SetValue(int row, int col, const wxString& strNewVal)
{
  u32 newVal = 0;
  if (col == 0 || TryParse("0x" + WxStrToStr(strNewVal), &newVal))
  {
    if (row > 0)
    {
      switch (col)
      {
      case 0:
      {
        SetWatchName(row, std::string(WxStrToStr(strNewVal)));
        break;
      }
      case 1:
      {
        if (row > (int)PowerPC::watches.GetWatches().size())
        {
          AddWatchAddr(row, newVal);
          row = (int)PowerPC::watches.GetWatches().size();
        }
        else
        {
          UpdateWatchAddr(row, newVal);
        }
        break;
      }
      case 2:
      {
        SetWatchValue(row, newVal);
        break;
      }
      default:
        break;
      }
    }
  }
}

void CWatchTable::UpdateWatch()
{
  for (int i = 0; i < (int)PowerPC::watches.GetWatches().size(); ++i)
  {
    m_CachedWatchHasChanged[i] = (m_CachedWatch[i] != GetWatchValue(i + 1));
    m_CachedWatch[i] = GetWatchValue(i + 1);
  }
}

wxGridCellAttr* CWatchTable::GetAttr(int row, int col, wxGridCellAttr::wxAttrKind)
{
  wxGridCellAttr* attr = new wxGridCellAttr();

  attr->SetBackgroundColour(*wxWHITE);
  attr->SetFont(DebuggerFont);

  switch (col)
  {
  case 1:
    attr->SetAlignment(wxALIGN_LEFT, wxALIGN_CENTER);
    break;
  case 3:
  case 4:
    attr->SetAlignment(wxALIGN_LEFT, wxALIGN_CENTER);
    break;
  default:
    attr->SetAlignment(wxALIGN_LEFT, wxALIGN_CENTER);
    break;
  }

  if (row == 0)
  {
    attr->SetReadOnly(true);
    attr->SetBackgroundColour(*wxBLACK);
    attr->SetTextColour(*wxWHITE);
  }
  else
  {
    bool red = false;
    if (col == 1)
      red = m_CachedWatchHasChanged[row];

    attr->SetTextColour(red ? *wxRED : *wxBLACK);

    if (row > (int)(PowerPC::watches.GetWatches().size() + 1) || !Core::IsRunning())
    {
      attr->SetReadOnly(true);
      attr->SetBackgroundColour(*wxLIGHT_GREY);
    }
  }

  return attr;
}

CWatchView::CWatchView(wxWindow* parent, wxWindowID id) : wxGrid(parent, id)
{
  m_watch_table = new CWatchTable();

  SetTable(m_watch_table, true);
  SetRowLabelSize(0);
  SetColLabelSize(0);
  DisableDragRowSize();

  Bind(wxEVT_GRID_CELL_RIGHT_CLICK, &CWatchView::OnMouseDownR, this);
  Bind(wxEVT_MENU, &CWatchView::OnPopupMenu, this);
}

void CWatchView::Repopulate()
{
  if (Core::IsRunning())
  {
    m_watch_table->UpdateWatch();
    ForceRefresh();
  }
}

void CWatchView::OnMouseDownR(wxGridEvent& event)
{
  // popup menu
  int row = event.GetRow();
  int col = event.GetCol();

  m_selectedRow = row;

  if (col == 1 || col == 2)
  {
    wxString strNewVal = GetValueByRowCol(row, col);
    TryParse("0x" + WxStrToStr(strNewVal), &m_selectedAddress);
  }

  wxMenu menu;
  if (row != 0 && row != (int)(PowerPC::watches.GetWatches().size() + 1))
  {
    // i18n: This kind of "watch" is used for watching emulated memory.
    // It's not related to timekeeping devices.
    menu.Append(IDM_DELETEWATCH, _("&Delete watch"));
  }

  if (row != 0 && row != (int)(PowerPC::watches.GetWatches().size() + 1) && (col == 1 || col == 2))
  {
    menu.Append(IDM_ADDMEMCHECK, _("Add memory &breakpoint"));
    menu.Append(IDM_VIEWMEMORY, _("View &memory"));
  }
  PopupMenu(&menu);
}

void CWatchView::OnPopupMenu(wxCommandEvent& event)
{
  // FIXME: This is terrible. Generate events instead.
  CFrame* cframe = wxGetApp().GetCFrame();
  CCodeWindow* code_window = cframe->m_code_window;
  CWatchWindow* watch_window = code_window->GetPanel<CWatchWindow>();
  CMemoryWindow* memory_window = code_window->GetPanel<CMemoryWindow>();
  CBreakPointWindow* breakpoint_window = code_window->GetPanel<CBreakPointWindow>();

  switch (event.GetId())
  {
  case IDM_DELETEWATCH:
  {
    wxString strNewVal = GetValueByRowCol(m_selectedRow, 1);
    if (TryParse("0x" + WxStrToStr(strNewVal), &m_selectedAddress))
    {
      PowerPC::watches.Remove(m_selectedAddress);
      if (watch_window)
        watch_window->NotifyUpdate();
      Refresh();
    }
    break;
  }
  case IDM_ADDMEMCHECK:
  {
    TMemCheck MemCheck;
    MemCheck.start_address = m_selectedAddress;
    MemCheck.end_address = m_selectedAddress;
    MemCheck.is_ranged = false;
    MemCheck.is_break_on_read = true;
    MemCheck.is_break_on_write = true;
    MemCheck.log_on_hit = true;
    MemCheck.break_on_hit = true;
    PowerPC::memchecks.Add(MemCheck);

    if (breakpoint_window)
      breakpoint_window->NotifyUpdate();
    Refresh();
    break;
  }
  case IDM_VIEWMEMORY:
    if (memory_window)
      memory_window->JumpToAddress(m_selectedAddress);
    Refresh();
    break;
  }
  event.Skip();
}
