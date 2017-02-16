// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/colour.h>
#include <wx/grid.h>

#include "Common/CommonTypes.h"
#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPTables.h"
#include "DolphinWX/Debugger/DSPRegisterView.h"
#include "DolphinWX/WxUtils.h"

wxString CDSPRegTable::GetValue(int row, int col)
{
  if (row < GetNumberRows())
  {
    switch (col)
    {
    case 0:
      return StrToWxStr(DSP::pdregname(row));
    case 1:
      return wxString::Format("0x%04x", DSP::DSPCore_ReadRegister(row));
    default:
      return wxEmptyString;
    }
  }
  return wxEmptyString;
}

void CDSPRegTable::SetValue(int, int, const wxString&)
{
}

void CDSPRegTable::UpdateCachedRegs()
{
  if (m_CachedCounter == DSP::g_dsp.step_counter)
  {
    return;
  }

  m_CachedCounter = DSP::g_dsp.step_counter;

  for (size_t i = 0; i < m_CachedRegs.size(); ++i)
  {
    const u16 value = DSP::DSPCore_ReadRegister(i);

    m_CachedRegHasChanged[i] = m_CachedRegs[i] != value;
    m_CachedRegs[i] = value;
  }
}

wxGridCellAttr* CDSPRegTable::GetAttr(int row, int col, wxGridCellAttr::wxAttrKind)
{
  wxGridCellAttr* attr = new wxGridCellAttr();

  attr->SetBackgroundColour(*wxWHITE);

  switch (col)
  {
  case 1:
    attr->SetAlignment(wxALIGN_CENTER, wxALIGN_CENTER);
    break;
  default:
    attr->SetAlignment(wxALIGN_LEFT, wxALIGN_CENTER);
    break;
  }

  if (col == 1)
    attr->SetTextColour(m_CachedRegHasChanged[row] ? *wxRED : *wxBLACK);

  return attr;
}

DSPRegisterView::DSPRegisterView(wxWindow* parent, wxWindowID id)
    : wxGrid(parent, id, wxDefaultPosition, wxDLG_UNIT(parent, wxSize(100, 80)))
{
  m_register_table = new CDSPRegTable();

  SetTable(m_register_table, true);
  SetRowLabelSize(0);
  SetColLabelSize(0);
  DisableDragRowSize();

  AutoSizeColumns();
}

void DSPRegisterView::Repopulate()
{
  m_register_table->UpdateCachedRegs();
  ForceRefresh();
}
