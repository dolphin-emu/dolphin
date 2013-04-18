// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "DSPDebugWindow.h"
#include "DSPRegisterView.h"
#include "../WxUtils.h"

wxString CDSPRegTable::GetValue(int row, int col)
{
	if (row < 32) // 32 "normal" regs
	{
		switch (col)
		{
		case 0: return StrToWxStr(pdregname(row));
		case 1: return wxString::Format(wxT("0x%04x"), DSPCore_ReadRegister(row));
		default: return wxEmptyString;
		}
	}
	return wxEmptyString;
}

void CDSPRegTable::SetValue(int, int, const wxString &)
{
}

void CDSPRegTable::UpdateCachedRegs()
{
	if (m_CachedCounter == g_dsp.step_counter)
	{
		return;
	}

	m_CachedCounter = g_dsp.step_counter;

	for (int i = 0; i < 32; ++i)
	{
		m_CachedRegHasChanged[i] = (m_CachedRegs[i] != DSPCore_ReadRegister(i));
		m_CachedRegs[i] = DSPCore_ReadRegister(i);
	}
}

wxGridCellAttr *CDSPRegTable::GetAttr(int row, int col, wxGridCellAttr::wxAttrKind)
{
	wxGridCellAttr *attr = new wxGridCellAttr();

	attr->SetBackgroundColour(wxColour(wxT("#FFFFFF")));

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
		attr->SetTextColour(m_CachedRegHasChanged[row] ? wxColor(wxT("#FF0000")) : wxColor(wxT("#000000")));

	attr->IncRef();
	return attr;
}

DSPRegisterView::DSPRegisterView(wxWindow *parent, wxWindowID id)
	: wxGrid(parent, id, wxDefaultPosition, wxSize(130, 120))
{
	SetTable(new CDSPRegTable(), true);
	SetRowLabelSize(0);
	SetColLabelSize(0);
	DisableDragRowSize();

	AutoSizeColumns();
}

void DSPRegisterView::Update()
{
	((CDSPRegTable *)GetTable())->UpdateCachedRegs();
	ForceRefresh();
}
