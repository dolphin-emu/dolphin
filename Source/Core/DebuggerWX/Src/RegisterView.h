// Copyright (C) 2003-2008 Dolphin Project.

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

class CRegTable
	: public wxGridTableBase
{
    public:
		CRegTable(){;}
        int GetNumberCols(void){return 4;}
        int GetNumberRows(void){return 16;}
		bool IsEmptyCell(int, int){return false;}
        wxString GetValue(int, int);
        void SetValue(int, int, const wxString &);
		wxGridCellAttr *GetAttr(int, int, wxGridCellAttr::wxAttrKind);
 
    private:
        DECLARE_NO_COPY_CLASS(CRegTable);
};

class CRegisterView
	: public wxGrid
{
	public:
		CRegisterView(wxWindow* parent, wxWindowID id);

		void Update();

		u32 m_CachedRegs[32];
		bool m_CachedRegHasChanged[32];
};
#endif
