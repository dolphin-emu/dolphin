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

#include "Debugger.h"
#include "RegisterView.h"
#include "PowerPC/PowerPC.h"

extern const char* GetGPRName(unsigned int index);


wxString CRegTable::GetValue(int row, int col)
{
	if (col % 2 == 0)
		return wxString::FromAscii(GetGPRName(16*col/2 + row));
	else
		return wxString::Format(wxT("0x%08x"), GPR(col == 1 ? row : 16 + row));
}

void CRegTable::SetValue(int, int, const wxString &)
{
}

wxGridCellAttr *CRegTable::GetAttr(int row, int col, wxGridCellAttr::wxAttrKind)
{
	wxGridCellAttr *attr = new wxGridCellAttr();

	attr->SetBackgroundColour(wxColour(wxT("#FFFFFF")));
	attr->SetFont(wxFont(9, wxMODERN, wxNORMAL, wxNORMAL, false, wxT("monospace")));
	attr->SetAlignment(col & 1 ? wxALIGN_CENTER : wxALIGN_LEFT, wxALIGN_CENTER);

	if (col % 2 == 0)
		attr->SetTextColour(wxColour(wxT("#000000")));
	else
		attr->SetTextColour(((CRegisterView*)GetView())->m_CachedRegHasChanged[col == 1 ? row : 16 + row]
			? wxColor(wxT("#FF0000")) : wxColor(wxT("#000000")));

	attr->IncRef();
	return attr;
}


CRegisterView::CRegisterView(wxWindow *parent, wxWindowID id)
	: wxGrid(parent, id)
{
	SetTable(new CRegTable(), true);
	EnableGridLines(false);
	SetRowLabelSize(0);
	SetColLabelSize(0);
	DisableDragGridSize();

	AutoSizeColumns();
}

void CRegisterView::Update()
{
	ForceRefresh();

	for (size_t i = 0; i < 32; ++i)
	{
		m_CachedRegHasChanged[i] = (m_CachedRegs[i] != GPR(i));
		m_CachedRegs[i] = GPR(i);
	}
}
