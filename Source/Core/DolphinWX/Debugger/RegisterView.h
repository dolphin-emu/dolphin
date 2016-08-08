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

class CRegTable;

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
