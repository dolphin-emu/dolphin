// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
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

#define NUM_SPECIALS 14

enum class FormatSpecifier;

class CRegTable : public wxGridTableBase
{
public:
  CRegTable();
  int GetNumberCols() override { return 9; }
  int GetNumberRows() override { return 32 + NUM_SPECIALS; }
  u32 GetSpecialRegValue(int reg);
  void SetSpecialRegValue(int reg, u32 value);
  bool IsEmptyCell(int row, int col) override { return row > 31 && col > 2; }
  wxString GetValue(int row, int col) override;
  wxString GetFormatString(FormatSpecifier specifier);
  wxString FormatGPR(int regIndex);
  wxString FormatFPR(int regIndex, int regPart);
  bool TryParseGPR(wxString str, FormatSpecifier format, u32* value);
  bool TryParseFPR(wxString str, FormatSpecifier format, unsigned long long int* value);
  void SetRegisterFormat(int col, int row, FormatSpecifier specifier);
  void SetValue(int row, int col, const wxString&) override;
  wxGridCellAttr* GetAttr(int, int, wxGridCellAttr::wxAttrKind) override;
  void UpdateCachedRegs();

private:
  u32 m_CachedRegs[32];
  u32 m_CachedSpecialRegs[NUM_SPECIALS];
  u64 m_CachedFRegs[32][2];
  bool m_CachedRegHasChanged[32];
  bool m_CachedSpecialRegHasChanged[NUM_SPECIALS];
  bool m_CachedFRegHasChanged[32][2];
  std::array<FormatSpecifier, 32> m_formatRegs;
  std::array<std::array<FormatSpecifier, 2>, 32> m_formatFRegs;

  DECLARE_NO_COPY_CLASS(CRegTable);
};

class CRegisterView : public wxGrid
{
public:
  CRegisterView(wxWindow* parent, wxWindowID id = wxID_ANY);
  void Update() override;

private:
  void OnMouseDownR(wxGridEvent& event);
  void OnPopupMenu(wxCommandEvent& event);

  u32 m_selectedAddress = 0;
  int m_selectedRow = 0;
  int m_selectedColumn = 0;

  // Owned by wx. Deleted implicitly upon destruction.
  CRegTable* m_register_table;
};
