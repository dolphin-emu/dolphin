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

#ifndef __DSPREGISTERVIEW_h__
#define __DSPREGISTERVIEW_h__

#include <wx/grid.h>


class CRegTable : public wxGridTableBase
{
private:
	u64 m_CachedCounter;
	u16 m_CachedRegs[32];
	bool m_CachedRegHasChanged[32];

	DECLARE_NO_COPY_CLASS(CRegTable);

public:
	CRegTable()
	{
		memset(m_CachedRegs, 0, sizeof(m_CachedRegs));
		memset(m_CachedRegHasChanged, 0, sizeof(m_CachedRegHasChanged));
	}

	int GetNumberCols(void) {return 2;}
	int GetNumberRows(void) {return 32;}
	bool IsEmptyCell(int row, int col) {return false;}
	wxString GetValue(int row, int col);
	void SetValue(int row, int col, const wxString &);
	wxGridCellAttr *GetAttr(int, int, wxGridCellAttr::wxAttrKind);
	void UpdateCachedRegs();
};

class DSPRegisterView : public wxGrid
{
public:
	DSPRegisterView(wxWindow* parent, wxWindowID id);
	void Update();
};

#endif //__DSPREGISTERVIEW_h__
