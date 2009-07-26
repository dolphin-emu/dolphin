// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef __REGISTERVIEW_h__
#define __REGISTERVIEW_h__

#include <wx/grid.h>

#include "Common.h"

// New register view:
// R0  0x8000000    F0   0.0000       F0_PS1 0.0000
// R1  0x8000000    F1   0.0000       F1_PS1 0.0000
// R31  0x8000000   F31  0.0000      F31_PS1 0.0000
// PC  (specials)
// LR
// CTR
// CR0-7
// FPSCR
// SRR0
// SRR1
// Exceptions

class CRegTable : public wxGridTableBase
{
	enum {
		NUM_SPECIALS = 10,
	};

public:
	CRegTable() {
		memset(m_CachedRegs, 0, sizeof(m_CachedRegs));
		memset(m_CachedSpecialRegs, 0, sizeof(m_CachedSpecialRegs));
		memset(m_CachedFRegs, 0, sizeof(m_CachedFRegs));
		memset(m_CachedRegHasChanged, 0, sizeof(m_CachedRegHasChanged));
		memset(m_CachedSpecialRegHasChanged, 0, sizeof(m_CachedSpecialRegHasChanged));
		memset(m_CachedFRegHasChanged, 0, sizeof(m_CachedFRegHasChanged));
	}
    int GetNumberCols(void) {return 5;}
    int GetNumberRows(void) {return 32 + NUM_SPECIALS;}
	bool IsEmptyCell(int row, int col) {return row > 31 && col > 2;}
    wxString GetValue(int row, int col);
    void SetValue(int row, int col, const wxString &);
	wxGridCellAttr *GetAttr(int, int, wxGridCellAttr::wxAttrKind);
	void UpdateCachedRegs();

private:
	u32 m_CachedRegs[32];
	u32 m_CachedSpecialRegs[6];
	double m_CachedFRegs[32][2];
	bool m_CachedRegHasChanged[32];
	bool m_CachedSpecialRegHasChanged[6];
	bool m_CachedFRegHasChanged[32][2];

	DECLARE_NO_COPY_CLASS(CRegTable);
};

class CRegisterView : public wxGrid
{
public:
	CRegisterView(wxWindow* parent, wxWindowID id);
	void Update();
};

#endif
