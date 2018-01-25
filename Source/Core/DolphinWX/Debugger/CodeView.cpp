// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <wx/brush.h>
#include <wx/clipbrd.h>
#include <wx/colour.h>
#include <wx/dataobj.h>
#include <wx/dcclient.h>
#include <wx/graphics.h>
#include <wx/menu.h>
#include <wx/pen.h>
#include <wx/textdlg.h>

#include "Common/CommonTypes.h"
#include "Common/DebugInterface.h"
#include "Common/StringUtil.h"
#include "Common/SymbolDB.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/Debugger/AssemblerEntryDialog.h"
#include "DolphinWX/Debugger/CodeView.h"
#include "DolphinWX/Debugger/DebuggerUIUtil.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/WxUtils.h"

wxDEFINE_EVENT(wxEVT_CODEVIEW_CHANGE, wxCommandEvent);

enum
{
  IDM_GOTOINMEMVIEW = 12000,
  IDM_COPYADDRESS,
  IDM_COPYHEX,
  IDM_COPYCODE,
  IDM_INSERTBLR,
  IDM_INSERTNOP,
  IDM_ASSEMBLE,
  IDM_RUNTOHERE,
  IDM_JITRESULTS,
  IDM_FOLLOWBRANCH,
  IDM_RENAMESYMBOL,
  IDM_SETSYMBOLSIZE,
  IDM_SETSYMBOLEND,
  IDM_PATCHALERT,
  IDM_COPYFUNCTION,
  IDM_ADDFUNCTION,
};

CCodeView::CCodeView(DebugInterface* debuginterface, SymbolDB* symboldb, wxWindow* parent,
                     wxWindowID Id)
    : wxControl(parent, Id), m_debugger(debuginterface), m_symbol_db(symboldb), m_plain(false),
      m_curAddress(debuginterface->GetPC()), m_align(debuginterface->GetInstructionSize(0)),
      m_rowHeight(FromDIP(13)), m_left_col_width(FromDIP(LEFT_COL_WIDTH)), m_selection(0),
      m_oldSelection(0), m_selecting(false)
{
  Bind(wxEVT_PAINT, &CCodeView::OnPaint, this);
  Bind(wxEVT_MOUSEWHEEL, &CCodeView::OnScrollWheel, this);
  Bind(wxEVT_LEFT_DOWN, &CCodeView::OnMouseDown, this);
  Bind(wxEVT_LEFT_UP, &CCodeView::OnMouseUpL, this);
  Bind(wxEVT_MOTION, &CCodeView::OnMouseMove, this);
  Bind(wxEVT_RIGHT_DOWN, &CCodeView::OnMouseDown, this);
  Bind(wxEVT_RIGHT_UP, &CCodeView::OnMouseUpR, this);
  Bind(wxEVT_MENU, &CCodeView::OnPopupMenu, this);
  Bind(wxEVT_SIZE, &CCodeView::OnResize, this);

  // Disable the erase event, the entire window is being painted so the erase
  // event will just cause unnecessary flicker.
  SetBackgroundStyle(wxBG_STYLE_PAINT);
#if defined(__WXMSW__) || defined(__WXGTK__)
  SetDoubleBuffered(true);
#endif
}

int CCodeView::YToAddress(int y)
{
  wxRect rc = GetClientRect();
  int ydiff = y - rc.height / 2 - m_rowHeight / 2;
  ydiff = (int)(floorf((float)ydiff / (float)m_rowHeight)) + 1;
  return m_curAddress + ydiff * m_align;
}

void CCodeView::OnMouseDown(wxMouseEvent& event)
{
  int x = event.m_x;
  int y = event.m_y;

  if (x > m_left_col_width)
  {
    m_oldSelection = m_selection;
    m_selection = YToAddress(y);
    // SetCapture(wnd);
    bool oldselecting = m_selecting;
    m_selecting = true;

    if (!oldselecting || (m_selection != m_oldSelection))
      Refresh();
  }
  else
  {
    ToggleBreakpoint(YToAddress(y));
  }

  event.Skip();
}

void CCodeView::OnScrollWheel(wxMouseEvent& event)
{
  const bool scroll_down = (event.GetWheelRotation() < 0);
  const int num_lines = event.GetLinesPerAction();

  if (scroll_down)
  {
    m_curAddress += num_lines * 4;
  }
  else
  {
    m_curAddress -= num_lines * 4;
  }

  Refresh();
  event.Skip();
}

void CCodeView::ToggleBreakpoint(u32 address)
{
  m_debugger->ToggleBreakpoint(address);
  Refresh();

  // Propagate back to the parent window to update the breakpoint list.
  wxCommandEvent evt(wxEVT_HOST_COMMAND, IDM_UPDATE_BREAKPOINTS);
  GetEventHandler()->AddPendingEvent(evt);
}

void CCodeView::OnMouseMove(wxMouseEvent& event)
{
  wxRect rc = GetClientRect();

  if (event.m_leftDown && event.m_x > m_left_col_width)
  {
    if (event.m_y < 0)
    {
      m_curAddress -= m_align;
      Refresh();
    }
    else if (event.m_y > rc.height)
    {
      m_curAddress += m_align;
      Refresh();
    }
    else
    {
      OnMouseDown(event);
    }
  }

  event.Skip();
}

void CCodeView::RaiseEvent()
{
  wxCommandEvent ev(wxEVT_CODEVIEW_CHANGE, GetId());
  ev.SetEventObject(this);
  ev.SetInt(m_selection);
  GetEventHandler()->ProcessEvent(ev);
}

void CCodeView::OnMouseUpL(wxMouseEvent& event)
{
  if (event.m_x > m_left_col_width)
  {
    m_curAddress = YToAddress(event.m_y);
    m_selecting = false;
    Refresh();
  }

  RaiseEvent();
  event.Skip();
}

u32 CCodeView::AddrToBranch(u32 addr)
{
  std::string disasm = m_debugger->Disassemble(addr);
  size_t pos = disasm.find("->0x");

  if (pos != std::string::npos)
  {
    std::string hex = disasm.substr(pos + 2);
    return std::stoul(hex, nullptr, 16);
  }

  return 0;
}

void CCodeView::InsertBlrNop(int Blr)
{
  // Check if this address has been modified
  int find = -1;
  for (u32 i = 0; i < m_blrList.size(); i++)
  {
    if (m_blrList.at(i).address == m_selection)
    {
      find = i;
      break;
    }
  }

  // Save the old value
  if (find >= 0)
  {
    m_debugger->WriteExtraMemory(0, m_blrList.at(find).oldValue, m_selection);
    m_blrList.erase(m_blrList.begin() + find);
  }
  else
  {
    BlrStruct temp;
    temp.address = m_selection;
    temp.oldValue = m_debugger->ReadMemory(m_selection);
    m_blrList.push_back(temp);
    if (Blr == 0)
      m_debugger->InsertBLR(m_selection, 0x4e800020);
    else
      m_debugger->InsertBLR(m_selection, 0x60000000);
  }
  Refresh();
}

void CCodeView::OnPopupMenu(wxCommandEvent& event)
{
  switch (event.GetId())
  {
  case IDM_GOTOINMEMVIEW:
    // CMemoryDlg::Goto(selection);
    break;

#if wxUSE_CLIPBOARD
  case IDM_COPYADDRESS:
  {
    wxClipboardLocker locker;
    wxTheClipboard->SetData(new wxTextDataObject(wxString::Format("%08x", m_selection)));
  }
  break;

  case IDM_COPYCODE:
  {
    wxClipboardLocker locker;
    std::string disasm = m_debugger->Disassemble(m_selection);
    wxTheClipboard->SetData(new wxTextDataObject(StrToWxStr(disasm)));
  }
  break;

  case IDM_COPYHEX:
  {
    wxClipboardLocker locker;
    wxTheClipboard->SetData(
        new wxTextDataObject(wxString::Format("%08x", m_debugger->ReadInstruction(m_selection))));
  }
  break;

  case IDM_COPYFUNCTION:
  {
    Symbol* symbol = m_symbol_db->GetSymbolFromAddr(m_selection);
    if (symbol)
    {
      std::string text;
      text = text + symbol->name + "\r\n";
      // we got a function
      u32 start = symbol->address;
      u32 end = start + symbol->size;
      for (u32 addr = start; addr != end; addr += 4)
      {
        std::string disasm = m_debugger->Disassemble(addr);
        text += StringFromFormat("%08x: ", addr) + disasm + "\r\n";
      }
      wxClipboardLocker locker;
      wxTheClipboard->SetData(new wxTextDataObject(StrToWxStr(text)));
    }
  }
  break;
#endif

  case IDM_RUNTOHERE:
    m_debugger->SetBreakpoint(m_selection);
    m_debugger->RunToBreakpoint();
    Refresh();
    break;

  // Insert blr or restore old value
  case IDM_INSERTBLR:
    InsertBlrNop(0);
    Refresh();
    break;

  case IDM_INSERTNOP:
    InsertBlrNop(1);
    Refresh();
    break;

  case IDM_ASSEMBLE:
  {
    if (!PowerPC::HostIsInstructionRAMAddress(m_selection))
      break;
    const PowerPC::TryReadInstResult read_result = PowerPC::TryReadInstruction(m_selection);
    if (!read_result.valid)
      break;
    AssemblerEntryDialog dialog(m_selection, this, _("Enter instruction code:"),
                                wxGetTextFromUserPromptStr,
                                wxString::Format(wxT("%#08x"), read_result.hex));
    if (dialog.ShowModal() == wxID_OK)
    {
      unsigned long code;
      if (dialog.GetValue().ToULong(&code, 0) && code <= std::numeric_limits<u32>::max())
      {
        m_debugger->InsertBLR(m_selection, code);
        Refresh();
      }
    }
    break;
  }

  case IDM_JITRESULTS:
  {
    // Propagate back to the parent window and tell it
    // to flip to the JIT tab for the current address.
    wxCommandEvent jit_event(wxEVT_HOST_COMMAND, IDM_UPDATE_JIT_PANE);
    GetEventHandler()->AddPendingEvent(jit_event);
  }
  break;

  case IDM_FOLLOWBRANCH:
  {
    u32 dest = AddrToBranch(m_selection);
    if (dest)
    {
      Center(dest);
      RaiseEvent();
    }
  }
  break;

  case IDM_ADDFUNCTION:
    m_symbol_db->AddFunction(m_selection);
    Host_NotifyMapLoaded();
    break;

  case IDM_RENAMESYMBOL:
  {
    Symbol* symbol = m_symbol_db->GetSymbolFromAddr(m_selection);
    if (symbol)
    {
      wxTextEntryDialog input_symbol(this, _("Rename symbol:"), wxGetTextFromUserPromptStr,
                                     StrToWxStr(symbol->name));
      if (input_symbol.ShowModal() == wxID_OK)
      {
        symbol->Rename(WxStrToStr(input_symbol.GetValue()));
        Refresh();  // Redraw to show the renamed symbol
      }
      Host_NotifyMapLoaded();
    }
  }
  break;

  case IDM_SETSYMBOLSIZE:
  {
    Symbol* symbol = m_symbol_db->GetSymbolFromAddr(m_selection);
    if (!symbol)
      break;

    wxTextEntryDialog dialog(this,
                             wxString::Format(_("Enter symbol (%s) size:"), symbol->name.c_str()),
                             wxGetTextFromUserPromptStr, wxString::Format(wxT("%i"), symbol->size));

    if (dialog.ShowModal() == wxID_OK)
    {
      unsigned long size;
      if (dialog.GetValue().ToULong(&size, 0) && size <= std::numeric_limits<u32>::max())
      {
        PPCAnalyst::ReanalyzeFunction(symbol->address, *symbol, size);
        Refresh();
        Host_NotifyMapLoaded();
      }
    }
  }
  break;

  case IDM_SETSYMBOLEND:
  {
    Symbol* symbol = m_symbol_db->GetSymbolFromAddr(m_selection);
    if (!symbol)
      break;

    wxTextEntryDialog dialog(
        this, wxString::Format(_("Enter symbol (%s) end address:"), symbol->name.c_str()),
        wxGetTextFromUserPromptStr, wxString::Format(wxT("%#08x"), symbol->address + symbol->size));

    if (dialog.ShowModal() == wxID_OK)
    {
      unsigned long address;
      if (dialog.GetValue().ToULong(&address, 0) && address <= std::numeric_limits<u32>::max() &&
          address >= symbol->address)
      {
        PPCAnalyst::ReanalyzeFunction(symbol->address, *symbol, address - symbol->address);
        Refresh();
        Host_NotifyMapLoaded();
      }
    }
  }
  break;

  case IDM_PATCHALERT:
    break;

  default:
    event.Skip();
    break;
  }
}

void CCodeView::OnMouseUpR(wxMouseEvent& event)
{
  bool isSymbol = m_symbol_db->GetSymbolFromAddr(m_selection) != nullptr;
  // popup menu
  wxMenu menu;
  // menu->Append(IDM_GOTOINMEMVIEW, "&Goto in mem view");
  menu.Append(IDM_FOLLOWBRANCH, _("Follow &branch"))
      ->Enable(AddrToBranch(m_selection) ? true : false);
  menu.AppendSeparator();
#if wxUSE_CLIPBOARD
  menu.Append(IDM_COPYADDRESS, _("&Copy address"));
  menu.Append(IDM_COPYFUNCTION, _("Copy &function"))->Enable(isSymbol);
  menu.Append(IDM_COPYCODE, _("Copy code &line"));
  menu.Append(IDM_COPYHEX, _("Copy &hex"));
  menu.AppendSeparator();
#endif
  menu.Append(IDM_RENAMESYMBOL, _("&Rename symbol"))->Enable(isSymbol);
  menu.Append(IDM_SETSYMBOLSIZE, _("Set symbol &size"))->Enable(isSymbol);
  menu.Append(IDM_SETSYMBOLEND, _("Set symbol &end address"))->Enable(isSymbol);
  menu.AppendSeparator();
  menu.Append(IDM_RUNTOHERE, _("Run &To Here"))->Enable(Core::IsRunning());
  menu.Append(IDM_ADDFUNCTION, _("&Add function"))->Enable(Core::IsRunning());
  menu.Append(IDM_JITRESULTS, _("PPC vs x86"))->Enable(Core::IsRunning());
  menu.Append(IDM_INSERTBLR, _("&Insert blr"))->Enable(Core::IsRunning());
  menu.Append(IDM_INSERTNOP, _("Insert &nop"))->Enable(Core::IsRunning());
  menu.Append(IDM_ASSEMBLE, _("Re&place Instruction"))->Enable(Core::IsRunning());
  // menu.Append(IDM_PATCHALERT, _("Patch alert"))->Enable(Core::IsRunning());
  PopupMenu(&menu);
  event.Skip();
}

void CCodeView::OnPaint(wxPaintEvent& event)
{
  // -------------------------
  // General settings
  // -------------------------
  wxPaintDC paint_dc(this);
  wxRect rc = GetClientRect();
  int char_width;

  paint_dc.SetFont(DebuggerFont);
  {
    wxFontMetrics metrics = paint_dc.GetFontMetrics();
    char_width = metrics.averageWidth;
    m_rowHeight = std::max(metrics.height, m_rowHeight);
    if (!DebuggerFont.IsFixedWidth())
      char_width = paint_dc.GetTextExtent("mxx").GetWidth() / 3;  // (1em + 2ex) / 3
  }

  std::unique_ptr<wxGraphicsContext> ctx(wxGraphicsContext::Create(paint_dc));
  ctx->DisableOffset();  // Incompatible with matrix transforms
  ctx->SetFont(DebuggerFont, *wxBLACK);

  struct Branch
  {
    int src, dst, srcAddr;
  };

  Branch branches[256];
  int num_branches = 0;
  const int num_rows = ((rc.height / m_rowHeight) / 2) + 2;

  const double scale = FromDIP(1024) / 1024.0;
  const int pen_width = static_cast<int>(std::ceil(scale));
  const int col_width = rc.width - m_left_col_width;
  const int text_col = m_left_col_width + pen_width / 2 + 1;  // 1 unscaled pixel
  const int bp_offset_x = FromDIP(LEFT_COL_WIDTH / 8);
  const wxSize bp_size = FromDIP(wxSize(LEFT_COL_WIDTH * 3 / 4, LEFT_COL_WIDTH * 3 / 4));
  const int bp_offset_y = (m_rowHeight - bp_size.GetHeight()) / 2;
  // ------------

  // -------------------------
  // Colors and brushes
  // -------------------------

  wxColour branch_color = wxTheColourDatabase->Find("PURPLE");
  wxColour blr_color = wxTheColourDatabase->Find("DARK GREEN");
  wxColour instr_color = wxTheColourDatabase->Find("VIOLET");
  wxGraphicsPen null_pen = ctx->CreatePen(*wxTRANSPARENT_PEN);
  wxGraphicsPen focus_pen = ctx->CreatePen(wxPen(*wxBLACK, pen_width));
  wxGraphicsPen selection_pen = ctx->CreatePen(wxPen("GREY", pen_width));
  wxGraphicsBrush pc_brush = ctx->CreateBrush(*wxGREEN_BRUSH);
  wxGraphicsBrush bp_brush = ctx->CreateBrush(*wxRED_BRUSH);
  wxGraphicsBrush back_brush = ctx->CreateBrush(*wxWHITE_BRUSH);
  wxGraphicsBrush null_brush = ctx->CreateBrush(*wxTRANSPARENT_BRUSH);

  // ------------

  // -----------------------------
  // Walk through all visible rows
  // -----------------------------
  for (int i = -num_rows; i <= num_rows; i++)
  {
    unsigned int address = m_curAddress + (i * m_align);

    int row_y = (rc.height / 2) + (m_rowHeight * i) - (m_rowHeight / 2);

    wxString temp = wxString::Format("%08x", address);
    u32 color = m_debugger->GetColor(address);
    wxBrush row_brush(wxColour(color >> 16, color >> 8, color));
    ctx->SetBrush(back_brush);
    ctx->SetPen(null_pen);
    ctx->DrawRectangle(0, row_y, m_left_col_width, m_rowHeight);

    if (address == m_debugger->GetPC())
      ctx->SetBrush(pc_brush);
    else
      ctx->SetBrush(row_brush);

    ctx->SetPen(null_pen);
    ctx->DrawRectangle(m_left_col_width, row_y, col_width, m_rowHeight);
    if (i == 0 || (m_selecting && address == m_selection))
    {
      if (m_selecting && address == m_selection)
        ctx->SetPen(selection_pen);
      else
        ctx->SetPen(focus_pen);
      ctx->SetBrush(null_brush);
      // In a graphics context, the border of a rectangle is drawn along the edge,
      // it does not count towards the width of the rectangle (i.e. drawn right on
      // the pixel boundary of the fill area, half inside, half outside. For example
      // a rect with a 1px pen at (5,5)->(10,10) will have an actual screen size of
      // (4.5,4.5)->(10.5,10.5) with the line being aliased on the half-pixels)
      double offset = pen_width / 2.0;
      ctx->DrawRectangle(m_left_col_width + offset, row_y + offset, col_width - pen_width,
                         m_rowHeight - pen_width);
    }

    if (!m_plain)
    {
      // the address text is dark red
      ctx->SetFont(DebuggerFont, wxColour(0x60, 0x00, 0x00));
      ctx->DrawText(temp, text_col, row_y);
      ctx->SetFont(DebuggerFont, *wxBLACK);
    }

    // If running
    if (m_debugger->IsAlive())
    {
      std::vector<std::string> dis = SplitString(m_debugger->Disassemble(address), '\t');
      dis.resize(2);

      static const size_t VALID_BRANCH_LENGTH = 10;
      const std::string& opcode = dis[0];
      const std::string& operands = dis[1];
      std::string desc;

      // look for hex strings to decode branches
      std::string hex_str;
      size_t pos = operands.find("0x");
      if (pos != std::string::npos)
      {
        hex_str = operands.substr(pos);
      }

      if (hex_str.length() == VALID_BRANCH_LENGTH)
      {
        u32 offs = std::stoul(hex_str, nullptr, 16);

        branches[num_branches].src = row_y + (m_rowHeight / 2);
        branches[num_branches].srcAddr = (address / m_align);
        branches[num_branches++].dst =
            (int)(row_y + ((s64)(u32)offs - (s64)(u32)address) * m_rowHeight / m_align +
                  m_rowHeight / 2);
        desc = StringFromFormat("-->%s", m_debugger->GetDescription(offs).c_str());

        // the -> arrow illustrations are purple
        ctx->SetFont(DebuggerFont, branch_color);
      }
      else
      {
        ctx->SetFont(DebuggerFont, *wxBLACK);
      }

      ctx->DrawText(StrToWxStr(operands), text_col + 17 * char_width, row_y);
      // ------------

      // Show blr as its' own color
      if (opcode == "blr")
        ctx->SetFont(DebuggerFont, blr_color);
      else
        ctx->SetFont(DebuggerFont, instr_color);

      ctx->DrawText(StrToWxStr(opcode), text_col + (m_plain ? 1 * char_width : 9 * char_width),
                    row_y);

      if (desc.empty())
      {
        desc = m_debugger->GetDescription(address);
      }

      if (!m_plain)
      {
        ctx->SetFont(DebuggerFont, *wxBLUE);

        // char temp[256];
        // UnDecorateSymbolName(desc,temp,255,UNDNAME_COMPLETE);
        if (!desc.empty())
        {
          ctx->DrawText(StrToWxStr(desc), text_col + 44 * char_width, row_y);
        }
      }

      // Show red breakpoint dot
      if (m_debugger->IsBreakpoint(address))
      {
        ctx->SetPen(null_pen);
        ctx->SetBrush(bp_brush);
        ctx->DrawEllipse(bp_offset_x, row_y + bp_offset_y, bp_size.GetWidth(), bp_size.GetHeight());
      }
    }
  }  // end of for
  // ------------

  // -------------------------
  // Colors and brushes
  // -------------------------
  ctx->SetPen(focus_pen);

  wxGraphicsPath branch_path = ctx->CreatePath();

  for (int i = 0; i < num_branches; ++i)
  {
    int x = text_col + 60 * char_width + (branches[i].srcAddr % 9) * 8;
    branch_path.MoveToPoint(x - 2 * scale, branches[i].src);

    if (branches[i].dst < rc.height + 400 && branches[i].dst > -400)
    {
      branch_path.AddLineToPoint(x + 2 * scale, branches[i].src);
      branch_path.AddLineToPoint(x + 2 * scale, branches[i].dst);
      branch_path.AddLineToPoint(x - 4 * scale, branches[i].dst);

      branch_path.MoveToPoint(x, branches[i].dst - 4 * scale);
      branch_path.AddLineToPoint(x - 4 * scale, branches[i].dst);
      branch_path.AddLineToPoint(x + 1 * scale, branches[i].dst + 5 * scale);
    }
    // else
    //{
    // This can be re-enabled when there is a scrollbar or
    // something on the codeview (the lines are too long)

    // LineTo(ctx, x+4, branches[i].src);
    // MoveTo(x+2, branches[i].dst-4);
    // LineTo(ctx, x+6, branches[i].dst);
    // LineTo(ctx, x+1, branches[i].dst+5);
    //}

    // LineTo(ctx, x, branches[i].dst+4);
    // LineTo(ctx, x-2, branches[i].dst);
  }

  // If the pen width is odd then we need to offset the path so that lines are drawn in
  // the middle of pixels instead of the edge so we don't get aliasing.
  if (pen_width & 1)
  {
    wxGraphicsMatrix matrix = ctx->CreateMatrix();
    matrix.Translate(0.5, 0.5);
    branch_path.Transform(matrix);
  }
  ctx->StrokePath(branch_path);
  // ------------
}

void CCodeView::OnResize(wxSizeEvent& event)
{
  Refresh();
  event.Skip();
}
