// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Debugger/MemoryView.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <locale>
#include <string>
#include <wx/brush.h>
#include <wx/clipbrd.h>
#include <wx/colour.h>
#include <wx/control.h>
#include <wx/dataobj.h>
#include <wx/dcclient.h>
#include <wx/font.h>
#include <wx/menu.h>
#include <wx/pen.h>

#include "Common/CommonTypes.h"
#include "Common/DebugInterface.h"
#include "Common/StringUtil.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/Debugger/CodeWindow.h"
#include "DolphinWX/Debugger/DebuggerUIUtil.h"
#include "DolphinWX/Debugger/WatchWindow.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/WxUtils.h"

enum
{
  IDM_GOTOINMEMVIEW = 12000,
  IDM_COPYADDRESS,
  IDM_COPYHEX,
  IDM_COPYCODE,
  IDM_RUNTOHERE,
  IDM_DYNARECRESULTS,
  IDM_WATCHADDRESS,
  IDM_TOGGLEMEMORY,
  IDM_VIEWASFP,
  IDM_VIEWASASCII,
  IDM_VIEWASHEX,
};

wxDEFINE_EVENT(DOLPHIN_EVT_MEMORY_VIEW_DATA_TYPE_CHANGED, wxCommandEvent);

CMemoryView::CMemoryView(DebugInterface* debuginterface, wxWindow* parent)
    : wxControl(parent, wxID_ANY), debugger(debuginterface), align(0), rowHeight(FromDIP(13)),
      m_left_col_width(FromDIP(LEFT_COL_WIDTH)), selection(0), oldSelection(0), selecting(false),
      memory(0), curAddress(debuginterface->GetPC()), m_data_type(MemoryDataType::U8)
{
  Bind(wxEVT_PAINT, &CMemoryView::OnPaint, this);
  Bind(wxEVT_LEFT_DOWN, &CMemoryView::OnMouseDownL, this);
  Bind(wxEVT_LEFT_UP, &CMemoryView::OnMouseUpL, this);
  Bind(wxEVT_MOTION, &CMemoryView::OnMouseMove, this);
  Bind(wxEVT_RIGHT_DOWN, &CMemoryView::OnMouseDownR, this);
  Bind(wxEVT_MOUSEWHEEL, &CMemoryView::OnScrollWheel, this);
  Bind(wxEVT_MENU, &CMemoryView::OnPopupMenu, this);
  Bind(wxEVT_SIZE, &CMemoryView::OnResize, this);

  SetDataType(MemoryDataType::FloatingPoint);

  // Every pixel will be drawn over in the paint event so erasing will just cause flickering.
  SetBackgroundStyle(wxBG_STYLE_PAINT);
#if defined(__WXMSW__) || defined(__WXGTK__)
  SetDoubleBuffered(true);
#endif
}

void CMemoryView::SetDataType(MemoryDataType data_type)
{
  if (m_data_type == data_type)
    return;

  m_data_type = data_type;
  switch (data_type)
  {
  case MemoryDataType::FloatingPoint:
  case MemoryDataType::ASCII:
    align = 4;
    break;
  default:
    align = 16;
    m_last_hex_type = data_type;
    break;
  }
  Refresh();

  wxCommandEvent ev(DOLPHIN_EVT_MEMORY_VIEW_DATA_TYPE_CHANGED, GetId());
  ev.SetInt(static_cast<int>(data_type));
  GetEventHandler()->ProcessEvent(ev);
}

int CMemoryView::YToAddress(int y)
{
  wxRect rc = GetClientRect();
  int ydiff = y - rc.height / 2 - rowHeight / 2;
  ydiff = (int)(floorf((float)ydiff / (float)rowHeight)) + 1;
  return curAddress + ydiff * align;
}

wxString CMemoryView::ReadMemoryAsString(u32 address) const
{
  std::string str;

  // FIXME: This doesn't work with the DSP Debugger
  u32 mem_data = debugger->ReadExtraMemory(memory, address);

  if (m_data_type == MemoryDataType::FloatingPoint)
  {
    float real;
    std::memcpy(&real, &mem_data, sizeof(u32));
    str = StringFromFormat("f: %f", real);
  }
  else if (m_data_type == MemoryDataType::ASCII)
  {
    str.reserve(4);
    for (unsigned int i = 0; i < 4; ++i)
    {
      char byte = static_cast<char>(mem_data >> (24 - i * 8) & 0xFF);
      if (std::isprint(byte, std::locale::classic()))
        str += byte;
      else
        str += ' ';
    }

    Symbol* sym = g_symbolDB.GetSymbolFromAddr(mem_data);
    if (sym)
    {
      str += StringFromFormat(" # -> %s", sym->name.c_str());
    }
  }
  else
  {
    str.reserve(48);
    for (unsigned int i = 0; i < align; i += sizeof(u32))
    {
      if (!PowerPC::HostIsRAMAddress(address + i))
        break;
      u32 word = debugger->ReadExtraMemory(memory, address + i);
      switch (m_data_type)
      {
      case MemoryDataType::U8:
      default:
        str += StringFromFormat(" %02X %02X %02X %02X", (word >> 24) & 0xFF, (word >> 16) & 0xFF,
                                (word >> 8) & 0xFF, word & 0xFF);
        break;
      case MemoryDataType::U16:
        str += StringFromFormat(" %04X %04X", (word >> 16) & 0xFFFF, word & 0xFFFF);
        break;
      case MemoryDataType::U32:
        str += StringFromFormat(" %08X", word);
        break;
      }
    }
  }

  // Not a UTF-8 string
  return wxString(str.c_str(), wxCSConv(wxFONTENCODING_CP1252), str.size());
}

void CMemoryView::OnMouseDownL(wxMouseEvent& event)
{
  int x = event.GetX();
  int y = event.GetY();

  if (x > m_left_col_width)
  {
    oldSelection = selection;
    selection = YToAddress(y);
    bool oldselecting = selecting;
    selecting = true;

    if (!oldselecting || (selection != oldSelection))
      Refresh();
  }
  else
  {
    debugger->ToggleMemCheck(YToAddress(y), memCheckRead, memCheckWrite, memCheckLog);

    Refresh();

    // Propagate back to the parent window to update the breakpoint list.
    wxCommandEvent evt(wxEVT_HOST_COMMAND, IDM_UPDATE_BREAKPOINTS);
    GetEventHandler()->AddPendingEvent(evt);
  }

  event.Skip();
}

void CMemoryView::OnMouseMove(wxMouseEvent& event)
{
  wxRect rc = GetClientRect();

  if (event.m_leftDown && event.m_x > m_left_col_width)
  {
    if (event.m_y < 0)
    {
      curAddress -= align;
      Refresh();
    }
    else if (event.m_y > rc.height)
    {
      curAddress += align;
      Refresh();
    }
    else
      OnMouseDownL(event);
  }

  event.Skip();
}

void CMemoryView::OnMouseUpL(wxMouseEvent& event)
{
  if (event.m_x > m_left_col_width)
  {
    curAddress = YToAddress(event.m_y);
    selecting = false;
    Refresh();
  }

  event.Skip();
}

void CMemoryView::OnScrollWheel(wxMouseEvent& event)
{
  const bool scroll_down = (event.GetWheelRotation() < 0);
  const int num_lines = event.GetLinesPerAction();

  if (scroll_down)
  {
    curAddress += num_lines * align;
  }
  else
  {
    curAddress -= num_lines * align;
  }

  Refresh();
  event.Skip();
}

void CMemoryView::OnPopupMenu(wxCommandEvent& event)
{
  // FIXME: This is terrible. Generate events instead.
  CFrame* cframe = wxGetApp().GetCFrame();
  CCodeWindow* code_window = cframe->m_code_window;
  CWatchWindow* watch_window = code_window->GetPanel<CWatchWindow>();

  switch (event.GetId())
  {
#if wxUSE_CLIPBOARD
  case IDM_COPYADDRESS:
  {
    wxClipboardLocker clipboard_lock;
    wxTheClipboard->SetData(new wxTextDataObject(wxString::Format("%08x", selection)));
  }
  break;

  case IDM_COPYHEX:
  {
    wxClipboardLocker clipboard_lock;
    wxTheClipboard->SetData(new wxTextDataObject(
        wxString::Format("%08x", debugger->ReadExtraMemory(memory, selection))));
  }
  break;
#endif

  case IDM_WATCHADDRESS:
    debugger->AddWatch(selection);
    if (watch_window)
      watch_window->NotifyUpdate();
    Refresh();
    break;

  case IDM_TOGGLEMEMORY:
    memory ^= 1;
    Refresh();
    break;

  case IDM_VIEWASFP:
    SetDataType(MemoryDataType::FloatingPoint);
    break;

  case IDM_VIEWASASCII:
    SetDataType(MemoryDataType::ASCII);
    break;

  case IDM_VIEWASHEX:
    SetDataType(m_last_hex_type);
    break;

  default:
    event.Skip();
    break;
  }
}

void CMemoryView::OnMouseDownR(wxMouseEvent& event)
{
  // popup menu
  wxMenu menu;
// menu.Append(IDM_GOTOINMEMVIEW, _("&Goto in mem view"));
#if wxUSE_CLIPBOARD
  menu.Append(IDM_COPYADDRESS, _("Copy &address"));
  menu.Append(IDM_COPYHEX, _("Copy &hex"));
#endif
  // i18n: This kind of "watch" is used for watching emulated memory.
  // It's not related to timekeeping devices.
  menu.Append(IDM_WATCHADDRESS, _("Add to &watch"));
  menu.AppendCheckItem(IDM_TOGGLEMEMORY, _("Toggle &memory"))->Check(memory != 0);

  wxMenu* viewAsSubMenu = new wxMenu;
  viewAsSubMenu->AppendRadioItem(IDM_VIEWASFP, _("FP value"))
      ->Check(m_data_type == MemoryDataType::FloatingPoint);
  viewAsSubMenu->AppendRadioItem(IDM_VIEWASASCII, "ASCII")
      ->Check(m_data_type == MemoryDataType::ASCII);
  viewAsSubMenu->AppendRadioItem(IDM_VIEWASHEX, _("Hex"))->Check(IsHexMode());
  menu.AppendSubMenu(viewAsSubMenu, _("View As:"));

  PopupMenu(&menu);
}

void CMemoryView::OnPaint(wxPaintEvent& event)
{
  wxPaintDC dc(this);
  wxRect rc = GetClientRect();

  dc.SetFont(DebuggerFont);

  int font_width;
  {
    wxFontMetrics metrics = dc.GetFontMetrics();
    font_width = metrics.averageWidth;
    rowHeight = std::max(rowHeight, metrics.height);
    if (!DebuggerFont.IsFixedWidth())
      font_width = dc.GetTextExtent("mxx").GetWidth() / 3;  // (1em + 2ex) / 3
  }

  const int row_start_x = m_left_col_width + 1;
  const int mchk_x = FromDIP(LEFT_COL_WIDTH / 8);
  const wxSize mchk_size = FromDIP(wxSize(LEFT_COL_WIDTH * 3 / 4, LEFT_COL_WIDTH * 3 / 4));
  const int mchk_offset_y = (rowHeight - mchk_size.GetHeight()) / 2;

  int col_width = rc.width - m_left_col_width;
  int num_rows = (rc.height / rowHeight) / 2 + 2;
  const wxColour navy_color = wxTheColourDatabase->Find("NAVY");

  const int pen_width = FromDIP(1);
  wxPen focus_pen(*wxBLACK, pen_width);
  wxPen selection_pen(*wxLIGHT_GREY, pen_width);
  wxBrush pc_brush(*wxGREEN_BRUSH);
  wxBrush mc_brush(*wxBLUE_BRUSH);
  wxBrush bg_brush(*wxWHITE_BRUSH);

  // TODO - clean up this freaking mess!!!!!
  for (int row = -num_rows; row <= num_rows; ++row)
  {
    u32 address = curAddress + row * align;

    int row_y = rc.height / 2 + rowHeight * row - rowHeight / 2;
    int row_x = row_start_x;

    auto draw_text = [&](const wxString& s, int offset_chars, int min_length) -> void {
      dc.DrawText(s, row_x + font_width * offset_chars, row_y);
      row_x += font_width * (std::max(static_cast<int>(s.size()), min_length) + offset_chars);
    };

    wxString temp = wxString::Format("%08x", address);
    u32 col = debugger->GetColor(address);
    wxBrush rowBrush(wxColour(col >> 16, col >> 8, col));
    dc.SetBrush(bg_brush);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(0, row_y, m_left_col_width, rowHeight);

    if (selecting && (address == selection))
      dc.SetPen(selection_pen);
    else
      dc.SetPen(row == 0 ? focus_pen : *wxTRANSPARENT_PEN);

    if (address == debugger->GetPC())
      dc.SetBrush(pc_brush);
    else
      dc.SetBrush(rowBrush);

    dc.DrawRectangle(m_left_col_width, row_y, col_width, rowHeight);
    dc.SetTextForeground(wxColour(0x60, 0x00, 0x00));  // Dark red
    draw_text(temp, 0, 0);

    if (!IsHexMode())
    {
      const std::string mem = debugger->GetRawMemoryString(memory, address);
      dc.SetTextForeground(navy_color);
      draw_text(StrToWxStr(mem), 2, 0);
    }
    dc.SetTextForeground(*wxBLACK);

    // NOTE: We can trigger a segfault inside HostIsRAMAddress (nullptr) during shutdown
    //   because we still get paint events even though the core is being deleted so we
    //   need to make sure the Memory still exists.
    // FIXME: This isn't relevant to the DSP Memory View
    if (!debugger->IsAlive() || !Memory::IsInitialized() || !PowerPC::HostIsRAMAddress(address))
      continue;

    // Pad to a minimum of 48 characters for full fixed point float width
    draw_text(ReadMemoryAsString(address), 2, 48);

    dc.SetTextForeground(*wxBLUE);

    std::string desc = debugger->GetDescription(address);
    if (!desc.empty())
      draw_text(StrToWxStr(desc), 2, 0);

    // Show blue memory check dot
    if (debugger->IsMemCheck(address, sizeof(u8)))
    {
      dc.SetPen(*wxTRANSPARENT_PEN);
      dc.SetBrush(mc_brush);
      dc.DrawEllipse(mchk_x, row_y + mchk_offset_y, mchk_size.GetWidth(), mchk_size.GetHeight());
    }
  }
}

void CMemoryView::OnResize(wxSizeEvent& event)
{
  Refresh();
  event.Skip();
}
