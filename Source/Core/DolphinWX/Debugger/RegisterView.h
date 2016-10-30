// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <wx/grid.h>

#include "Common/CommonTypes.h"

// New register view:
// R0  0x8000000    F0   0.0000       F0_PS1 0.0000
// R1  0x8000000    F1   0.0000       F1_PS1 0.0000
// R31  0x8000000   F31  0.0000      F31_PS1 0.0000
// PC  (specials)
// LR
// CTR
// CR0-7
// FPSCR
// MSR
// SRR0
// SRR1
// Exceptions
// Interrupt Mask (PI)
// Interrupt Cause(PI)

class CRegTable : public wxGridTableBase
{
public:
  enum class FormatSpecifier
  {
    Hex8,
    Hex16,
    Float,
    Double,
    UInt,
    Int
  };

  CRegTable();
  int GetNumberCols() override { return 9; }
  int GetNumberRows() override { return 32 + NUM_SPECIALS; }
  bool IsEmptyCell(int row, int col) override { return row > 31 && col > 2; }
  wxString GetValue(int row, int col) override;
  void SetValue(int row, int col, const wxString&) override;
  wxGridCellAttr* GetAttr(int, int, wxGridCellAttr::wxAttrKind) override;
  void SetRegisterFormat(int col, int row, FormatSpecifier specifier);
  void UpdateCachedRegs();

private:
  static constexpr int NUM_SPECIALS = 14;

  std::array<u32, 32> m_CachedRegs{};
  std::array<u32, NUM_SPECIALS> m_CachedSpecialRegs{};
  std::array<std::array<u64, 2>, 32> m_CachedFRegs{};
  std::array<bool, 32> m_CachedRegHasChanged{};
  std::array<bool, NUM_SPECIALS> m_CachedSpecialRegHasChanged{};
  std::array<std::array<bool, 2>, 32> m_CachedFRegHasChanged{};
  std::array<FormatSpecifier, 32> m_formatRegs{};
  std::array<std::array<FormatSpecifier, 2>, 32> m_formatFRegs;

  wxString FormatGPR(int reg_index);
  wxString FormatFPR(int reg_index, int reg_part);

  DECLARE_NO_COPY_CLASS(CRegTable);
};

class CRegisterView : public wxGrid
{
public:
  CRegisterView(wxWindow* parent, wxWindowID id = wxID_ANY);
  void Repopulate();

private:
  void OnMouseDownR(wxGridEvent& event);
  void OnPopupMenu(wxCommandEvent& event);

  u32 m_selectedAddress = 0;
  int m_selectedRow = 0;
  int m_selectedColumn = 0;

  // Owned by wx. Deleted implicitly upon destruction.
  CRegTable* m_register_table;
};
