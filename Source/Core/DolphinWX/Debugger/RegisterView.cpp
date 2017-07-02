// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/colour.h>
#include <wx/grid.h>
#include <wx/menu.h>

#include "Common/CommonTypes.h"
#include "Common/GekkoDisassembler.h"
#include "Common/StringUtil.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/Debugger/CodeWindow.h"
#include "DolphinWX/Debugger/DebuggerUIUtil.h"
#include "DolphinWX/Debugger/MemoryWindow.h"
#include "DolphinWX/Debugger/RegisterView.h"
#include "DolphinWX/Debugger/WatchWindow.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/WxUtils.h"

// F-zero 80005e60 wtf??

namespace
{
enum
{
  IDM_WATCHADDRESS,
  IDM_VIEWMEMORY,
  IDM_VIEWCODE,
  IDM_VIEW_HEX8,
  IDM_VIEW_HEX16,
  IDM_VIEW_FLOAT,
  IDM_VIEW_DOUBLE,
  IDM_VIEW_UINT,
  IDM_VIEW_INT
};

constexpr const char* special_reg_names[] = {"PC",        "LR",    "CTR",  "CR",         "FPSCR",
                                             "MSR",       "SRR0",  "SRR1", "Exceptions", "Int Mask",
                                             "Int Cause", "DSISR", "DAR",  "PT hashmask"};

wxString GetFormatString(CRegTable::FormatSpecifier specifier)
{
  switch (specifier)
  {
  case CRegTable::FormatSpecifier::Hex8:
    return wxString("%08x");
  case CRegTable::FormatSpecifier::Hex16:
    return wxString("%016llx");
  case CRegTable::FormatSpecifier::Float:
    return wxString("%g");
  case CRegTable::FormatSpecifier::Double:
    return wxString("%g");
  case CRegTable::FormatSpecifier::UInt:
    return wxString("%u");
  case CRegTable::FormatSpecifier::Int:
    return wxString("%i");
  default:
    return wxString("");
  }
}

u32 GetSpecialRegValue(int reg)
{
  switch (reg)
  {
  case 0:
    return PowerPC::ppcState.pc;
  case 1:
    return PowerPC::ppcState.spr[SPR_LR];
  case 2:
    return PowerPC::ppcState.spr[SPR_CTR];
  case 3:
    return GetCR();
  case 4:
    return PowerPC::ppcState.fpscr;
  case 5:
    return PowerPC::ppcState.msr;
  case 6:
    return PowerPC::ppcState.spr[SPR_SRR0];
  case 7:
    return PowerPC::ppcState.spr[SPR_SRR1];
  case 8:
    return PowerPC::ppcState.Exceptions;
  case 9:
    return ProcessorInterface::GetMask();
  case 10:
    return ProcessorInterface::GetCause();
  case 11:
    return PowerPC::ppcState.spr[SPR_DSISR];
  case 12:
    return PowerPC::ppcState.spr[SPR_DAR];
  case 13:
    return (PowerPC::ppcState.pagetable_hashmask << 6) | PowerPC::ppcState.pagetable_base;
  default:
    return 0;
  }
}

void SetSpecialRegValue(int reg, u32 value)
{
  switch (reg)
  {
  case 0:
    PowerPC::ppcState.pc = value;
    break;
  case 1:
    PowerPC::ppcState.spr[SPR_LR] = value;
    break;
  case 2:
    PowerPC::ppcState.spr[SPR_CTR] = value;
    break;
  case 3:
    SetCR(value);
    break;
  case 4:
    PowerPC::ppcState.fpscr = value;
    break;
  case 5:
    PowerPC::ppcState.msr = value;
    break;
  case 6:
    PowerPC::ppcState.spr[SPR_SRR0] = value;
    break;
  case 7:
    PowerPC::ppcState.spr[SPR_SRR1] = value;
    break;
  case 8:
    PowerPC::ppcState.Exceptions = value;
    break;
  // Should we just change the value, or use ProcessorInterface::SetInterrupt() to make the system
  // aware?
  // case 9: return ProcessorInterface::GetMask();
  // case 10: return ProcessorInterface::GetCause();
  case 11:
    PowerPC::ppcState.spr[SPR_DSISR] = value;
    break;
  case 12:
    PowerPC::ppcState.spr[SPR_DAR] = value;
    break;
  // case 13: (PowerPC::ppcState.pagetable_hashmask << 6) | PowerPC::ppcState.pagetable_base;
  default:
    return;
  }
}

bool TryParseFPR(wxString str, CRegTable::FormatSpecifier format, unsigned long long* value)
{
  if (format == CRegTable::FormatSpecifier::Hex16)
    return str.ToULongLong(value, 16);

  if (format == CRegTable::FormatSpecifier::Double)
  {
    double double_val;
    if (str.ToCDouble(&double_val))
    {
      std::memcpy(value, &double_val, sizeof(u64));
      return true;
    }

    return false;
  }

  return false;
}

bool TryParseGPR(wxString str, CRegTable::FormatSpecifier format, u32* value)
{
  if (format == CRegTable::FormatSpecifier::Hex8)
  {
    unsigned long val;
    if (str.ToCULong(&val, 16))
    {
      *value = static_cast<u32>(val);
      return true;
    }
    return false;
  }

  if (format == CRegTable::FormatSpecifier::Int)
  {
    long signed_val;
    if (str.ToCLong(&signed_val))
    {
      *value = static_cast<u32>(signed_val);
      return true;
    }
    return false;
  }

  if (format == CRegTable::FormatSpecifier::UInt)
  {
    unsigned long val;
    if (str.ToCULong(&val))
    {
      *value = static_cast<u32>(val);
      return true;
    }
    return false;
  }

  if (format == CRegTable::FormatSpecifier::Float)
  {
    double double_val;
    if (str.ToCDouble(&double_val))
    {
      float float_val = static_cast<float>(double_val);
      std::memcpy(value, &float_val, sizeof(float));
      return true;
    }
    return false;
  }
  return false;
}

}  // Anonymous namespace

CRegTable::CRegTable()
{
  for (auto& entry : m_formatFRegs)
    entry.fill(FormatSpecifier::Hex16);
}

wxString CRegTable::FormatGPR(int reg_index)
{
  if (m_formatRegs[reg_index] == FormatSpecifier::Int)
  {
    return wxString::Format(GetFormatString(m_formatRegs[reg_index]),
                            static_cast<s32>(GPR(reg_index)));
  }
  if (m_formatRegs[reg_index] == FormatSpecifier::Float)
  {
    float value;
    std::memcpy(&value, &GPR(reg_index), sizeof(float));
    return wxString::Format(GetFormatString(m_formatRegs[reg_index]), value);
  }
  return wxString::Format(GetFormatString(m_formatRegs[reg_index]), GPR(reg_index));
}

wxString CRegTable::FormatFPR(int reg_index, int reg_part)
{
  if (m_formatFRegs[reg_index][reg_part] == FormatSpecifier::Double)
  {
    double reg = (reg_part == 0) ? rPS0(reg_index) : rPS1(reg_index);
    return wxString::Format(GetFormatString(m_formatFRegs[reg_index][reg_part]), reg);
  }
  u64 reg = (reg_part == 0) ? riPS0(reg_index) : riPS1(reg_index);
  return wxString::Format(GetFormatString(m_formatFRegs[reg_index][reg_part]), reg);
}

void CRegTable::SetRegisterFormat(int col, int row, FormatSpecifier specifier)
{
  if (row >= 32)
    return;

  switch (col)
  {
  case 1:
    m_formatRegs[row] = specifier;
    return;
  case 3:
    m_formatFRegs[row][0] = specifier;
    return;
  case 4:
    m_formatFRegs[row][1] = specifier;
    return;
  }
}

wxString CRegTable::GetValue(int row, int col)
{
  if (row < 32)
  {
    switch (col)
    {
    case 0:
      return StrToWxStr(GekkoDisassembler::GetGPRName(row));
    case 1:
      return FormatGPR(row);
    case 2:
      return StrToWxStr(GekkoDisassembler::GetFPRName(row));
    case 3:
      return FormatFPR(row, 0);
    case 4:
      return FormatFPR(row, 1);
    case 5:
    {
      if (row < 4)
        return wxString::Format("DBAT%01d", row);

      if (row < 8)
        return wxString::Format("IBAT%01d", row - 4);

      if (row < 12)
        return wxString::Format("DBAT%01d", row - 4);

      if (row < 16)
        return wxString::Format("IBAT%01d", row - 8);

      break;
    }
    case 6:
    {
      if (row < 4)
        return wxString::Format("%016llx", (u64)PowerPC::ppcState.spr[SPR_DBAT0U + row * 2] << 32 |
                                               PowerPC::ppcState.spr[SPR_DBAT0L + row * 2]);

      if (row < 8)
        return wxString::Format("%016llx", (u64)PowerPC::ppcState.spr[SPR_IBAT0U + (row - 4) * 2]
                                                   << 32 |
                                               PowerPC::ppcState.spr[SPR_IBAT0L + (row - 4) * 2]);

      if (row < 12)
        return wxString::Format("%016llx", (u64)PowerPC::ppcState.spr[SPR_DBAT4U + (row - 12) * 2]
                                                   << 32 |
                                               PowerPC::ppcState.spr[SPR_DBAT4L + (row - 12) * 2]);

      if (row < 16)
        return wxString::Format("%016llx", (u64)PowerPC::ppcState.spr[SPR_IBAT4U + (row - 16) * 2]
                                                   << 32 |
                                               PowerPC::ppcState.spr[SPR_IBAT4L + (row - 16) * 2]);

      break;
    }
    case 7:
    {
      if (row < 16)
        return wxString::Format("SR%02d", row);

      break;
    }
    case 8:
    {
      if (row < 16)
        return wxString::Format("%08x", PowerPC::ppcState.sr[row]);

      break;
    }
    default:
      return wxEmptyString;
    }
  }
  else
  {
    if (static_cast<size_t>(row - 32) < NUM_SPECIALS)
    {
      switch (col)
      {
      case 0:
        return StrToWxStr(special_reg_names[row - 32]);
      case 1:
        return wxString::Format("%08x", GetSpecialRegValue(row - 32));
      default:
        return wxEmptyString;
      }
    }
  }
  return wxEmptyString;
}

void CRegTable::SetValue(int row, int col, const wxString& strNewVal)
{
  if (row < 32)
  {
    if (col == 1)
    {
      u32 new_val = 0;
      if (TryParseGPR(strNewVal, m_formatRegs[row], &new_val))
        GPR(row) = new_val;
    }
    else if (col == 3)
    {
      unsigned long long new_val = 0;
      if (TryParseFPR(strNewVal, m_formatFRegs[row][0], &new_val))
        riPS0(row) = new_val;
    }
    else if (col == 4)
    {
      unsigned long long new_val = 0;
      if (TryParseFPR(strNewVal, m_formatFRegs[row][1], &new_val))
        riPS1(row) = new_val;
    }
  }
  else
  {
    if ((static_cast<size_t>(row - 32) < NUM_SPECIALS) && col == 1)
    {
      u32 new_val = 0;
      if (TryParse("0x" + WxStrToStr(strNewVal), &new_val))
        SetSpecialRegValue(row - 32, new_val);
    }
  }
}

void CRegTable::UpdateCachedRegs()
{
  for (int i = 0; i < 32; ++i)
  {
    m_CachedRegHasChanged[i] = (m_CachedRegs[i] != GPR(i));
    m_CachedRegs[i] = GPR(i);

    m_CachedFRegHasChanged[i][0] = (m_CachedFRegs[i][0] != riPS0(i));
    m_CachedFRegs[i][0] = riPS0(i);
    m_CachedFRegHasChanged[i][1] = (m_CachedFRegs[i][1] != riPS1(i));
    m_CachedFRegs[i][1] = riPS1(i);
  }
  for (size_t i = 0; i < NUM_SPECIALS; ++i)
  {
    m_CachedSpecialRegHasChanged[i] = (m_CachedSpecialRegs[i] != GetSpecialRegValue(i));
    m_CachedSpecialRegs[i] = GetSpecialRegValue(i);
  }
}

wxGridCellAttr* CRegTable::GetAttr(int row, int col, wxGridCellAttr::wxAttrKind)
{
  wxGridCellAttr* attr = new wxGridCellAttr();

  attr->SetBackgroundColour(*wxWHITE);
  attr->SetFont(DebuggerFont);

  switch (col)
  {
  case 1:
    attr->SetAlignment(wxALIGN_CENTER, wxALIGN_CENTER);
    break;
  case 3:
  case 4:
    attr->SetAlignment(wxALIGN_RIGHT, wxALIGN_CENTER);
    break;
  default:
    attr->SetAlignment(wxALIGN_LEFT, wxALIGN_CENTER);
    break;
  }

  bool red = false;
  switch (col)
  {
  case 1:
    red = row < 32 ? m_CachedRegHasChanged[row] : m_CachedSpecialRegHasChanged[row - 32];
    break;
  case 3:
  case 4:
    red = row < 32 ? m_CachedFRegHasChanged[row][col - 3] : false;
    break;
  }

  attr->SetTextColour(red ? *wxRED : *wxBLACK);
  return attr;
}

CRegisterView::CRegisterView(wxWindow* parent, wxWindowID id) : wxGrid(parent, id)
{
  m_register_table = new CRegTable();

  SetTable(m_register_table, true);
  SetRowLabelSize(0);
  SetColLabelSize(0);
  DisableDragRowSize();

  Bind(wxEVT_GRID_CELL_RIGHT_CLICK, &CRegisterView::OnMouseDownR, this);
  Bind(wxEVT_MENU, &CRegisterView::OnPopupMenu, this);

  AutoSizeColumns();
}

void CRegisterView::Repopulate()
{
  m_register_table->UpdateCachedRegs();
  ForceRefresh();
}

void CRegisterView::OnMouseDownR(wxGridEvent& event)
{
  // popup menu
  m_selectedRow = event.GetRow();
  m_selectedColumn = event.GetCol();

  wxString strNewVal = m_register_table->GetValue(m_selectedRow, m_selectedColumn);
  TryParse("0x" + WxStrToStr(strNewVal), &m_selectedAddress);

  wxMenu menu;
  // i18n: This kind of "watch" is used for watching emulated memory.
  // It's not related to timekeeping devices.
  menu.Append(IDM_WATCHADDRESS, _("Add to &watch"));
  menu.Append(IDM_VIEWMEMORY, _("View &memory"));
  menu.Append(IDM_VIEWCODE, _("View &code"));
  if (m_selectedRow < 32 &&
      (m_selectedColumn == 1 || m_selectedColumn == 3 || m_selectedColumn == 4))
  {
    menu.AppendSeparator();
    if (m_selectedColumn == 1)
    {
      menu.Append(IDM_VIEW_HEX8, _("View as hexadecimal"));
      menu.Append(IDM_VIEW_INT, _("View as signed integer"));
      menu.Append(IDM_VIEW_UINT, _("View as unsigned integer"));
      // i18n: Float means floating point number
      menu.Append(IDM_VIEW_FLOAT, _("View as float"));
    }
    else
    {
      menu.Append(IDM_VIEW_HEX16, _("View as hexadecimal"));
      // i18n: Double means double-precision floating point number
      menu.Append(IDM_VIEW_DOUBLE, _("View as double"));
    }
  }
  PopupMenu(&menu);
}

void CRegisterView::OnPopupMenu(wxCommandEvent& event)
{
  // FIXME: This is terrible. Generate events instead.
  CFrame* cframe = wxGetApp().GetCFrame();
  CCodeWindow* code_window = cframe->m_code_window;
  CWatchWindow* watch_window = code_window->GetPanel<CWatchWindow>();
  CMemoryWindow* memory_window = code_window->GetPanel<CMemoryWindow>();

  switch (event.GetId())
  {
  case IDM_WATCHADDRESS:
    PowerPC::watches.Add(m_selectedAddress);
    if (watch_window)
      watch_window->NotifyUpdate();
    Refresh();
    break;
  case IDM_VIEWMEMORY:
    if (memory_window)
      memory_window->JumpToAddress(m_selectedAddress);
    Refresh();
    break;
  case IDM_VIEWCODE:
    code_window->JumpToAddress(m_selectedAddress);
    Refresh();
    break;
  case IDM_VIEW_HEX8:
    m_register_table->SetRegisterFormat(m_selectedColumn, m_selectedRow,
                                        CRegTable::FormatSpecifier::Hex8);
    Refresh();
    break;
  case IDM_VIEW_HEX16:
    m_register_table->SetRegisterFormat(m_selectedColumn, m_selectedRow,
                                        CRegTable::FormatSpecifier::Hex16);
    Refresh();
    break;
  case IDM_VIEW_INT:
    m_register_table->SetRegisterFormat(m_selectedColumn, m_selectedRow,
                                        CRegTable::FormatSpecifier::Int);
    Refresh();
    break;
  case IDM_VIEW_UINT:
    m_register_table->SetRegisterFormat(m_selectedColumn, m_selectedRow,
                                        CRegTable::FormatSpecifier::UInt);
    Refresh();
    break;
  case IDM_VIEW_FLOAT:
    m_register_table->SetRegisterFormat(m_selectedColumn, m_selectedRow,
                                        CRegTable::FormatSpecifier::Float);
    Refresh();
    break;
  case IDM_VIEW_DOUBLE:
    m_register_table->SetRegisterFormat(m_selectedColumn, m_selectedRow,
                                        CRegTable::FormatSpecifier::Double);
    Refresh();
    break;
  }
  event.Skip();
}
