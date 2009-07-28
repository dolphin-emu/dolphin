// Copyright (C) 2003 Dolphin Project.

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
#include "HW/PeripheralInterface.h"

// F-zero 80005e60 wtf??

extern const char* GetGPRName(unsigned int index);
extern const char* GetFPRName(unsigned int index);

static const char *special_reg_names[] = {
	"PC", "LR", "CTR", "CR", "FPSCR", "SRR0", "SRR1", "Exceptions", "Int Mask", "Int Cause",
};

static u32 GetSpecialRegValue(int reg) {
	switch (reg) {
	case 0: return PowerPC::ppcState.pc;
	case 1: return PowerPC::ppcState.spr[SPR_LR];
	case 2: return PowerPC::ppcState.spr[SPR_CTR];
	case 3: return GetCR();
	case 4: return PowerPC::ppcState.fpscr;
	case 5: return PowerPC::ppcState.spr[SPR_SRR0];
	case 6: return PowerPC::ppcState.spr[SPR_SRR1];
	case 7: return PowerPC::ppcState.Exceptions;
	case 8: return CPeripheralInterface::GetMask();
	case 9: return CPeripheralInterface::GetCause();
	default: return 0;	
	}
}

wxString CRegTable::GetValue(int row, int col)
{
	if (row < 32) {
		switch (col) {
		case 0: return wxString::FromAscii(GetGPRName(row));
		case 1: return wxString::Format(wxT("0x%08x"), GPR(row));
		case 2: return wxString::FromAscii(GetFPRName(row));
		case 3: return wxString::Format(wxT("%f"), rPS0(row));
		case 4: return wxString::Format(wxT("%f"), rPS1(row));
		default: return wxString::FromAscii("");
		}
	} else {
		if (row - 32 < NUM_SPECIALS) {
			switch (col) {
			case 0:	return wxString::FromAscii(special_reg_names[row - 32]);
			case 1: return wxString::Format(wxT("0x%08x"), GetSpecialRegValue(row - 32));
			default: return wxString::FromAscii("");
		    }
		}
	}
    return wxString::FromAscii("");
}

void CRegTable::SetValue(int, int, const wxString &)
{
}

void CRegTable::UpdateCachedRegs()
{
	for (int i = 0; i < 32; ++i)
	{
		m_CachedRegHasChanged[i] = (m_CachedRegs[i] != GPR(i));
		m_CachedRegs[i] = GPR(i);

		m_CachedFRegHasChanged[i][0] = (m_CachedFRegs[i][0] != rPS0(i));
		m_CachedFRegs[i][0] = rPS0(i);
		m_CachedFRegHasChanged[i][1] = (m_CachedFRegs[i][1] != rPS1(i));
		m_CachedFRegs[i][1] = rPS1(i);
	}
	for (int i = 0; i < 6; ++i)
	{
		m_CachedSpecialRegHasChanged[i] = (m_CachedSpecialRegs[i] != GetSpecialRegValue(i));
		m_CachedSpecialRegs[i] = GetSpecialRegValue(i);
	}
}

wxGridCellAttr *CRegTable::GetAttr(int row, int col, wxGridCellAttr::wxAttrKind)
{
	wxGridCellAttr *attr = new wxGridCellAttr();

	attr->SetBackgroundColour(wxColour(wxT("#FFFFFF")));
	attr->SetFont(DebuggerFont);

	switch (col) {
	case 1:
	case 3:
	case 4:
		attr->SetAlignment(wxALIGN_CENTER, wxALIGN_CENTER);
		break;
	default:
		attr->SetAlignment(wxALIGN_LEFT, wxALIGN_CENTER);
		break;
	}

	bool red = false;
	switch (col) {
	case 1: red = row < 32 ? m_CachedRegHasChanged[row] : m_CachedSpecialRegHasChanged[row-32]; break;
	case 3: 
	case 4: red = row < 32 ? m_CachedFRegHasChanged[row][col-3] : false; break;
	}
	attr->SetTextColour(red ? wxColor(wxT("#FF0000")) : wxColor(wxT("#000000")));
	attr->IncRef();
	return attr;
}

CRegisterView::CRegisterView(wxWindow *parent, wxWindowID id)
	: wxGrid(parent, id)
{
	SetTable(new CRegTable(), true);
	SetRowLabelSize(0);
	SetColLabelSize(0);
	DisableDragRowSize();

	AutoSizeColumns();
}

void CRegisterView::Update()
{
	ForceRefresh();
	((CRegTable *)GetTable())->UpdateCachedRegs();
}
