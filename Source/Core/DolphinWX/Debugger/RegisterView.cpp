// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <wx/chartype.h>
#include <wx/colour.h>
#include <wx/defs.h>
#include <wx/grid.h>
#include <wx/menu.h>
#include <wx/string.h>
#include <wx/windowid.h>

#include "Common/CommonTypes.h"
#include "Common/GekkoDisassembler.h"
#include "Common/StringUtil.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/CodeWindow.h"
#include "DolphinWX/Debugger/DebuggerUIUtil.h"
#include "DolphinWX/Debugger/MemoryWindow.h"
#include "DolphinWX/Debugger/RegisterView.h"
#include "DolphinWX/Debugger/WatchWindow.h"

// F-zero 80005e60 wtf??

enum
{
	IDM_WATCHADDRESS,
	IDM_VIEWMEMORY,
};

static const char *special_reg_names[] = {
	"PC", "LR", "CTR", "CR", "FPSCR", "MSR", "SRR0", "SRR1", "Exceptions", "Int Mask", "Int Cause", "DSISR", "DAR", "PT hashmask"
};

static u32 GetSpecialRegValue(int reg)
{
	switch (reg)
	{
	case 0: return PowerPC::ppcState.pc;
	case 1: return PowerPC::ppcState.spr[SPR_LR];
	case 2: return PowerPC::ppcState.spr[SPR_CTR];
	case 3: return GetCR();
	case 4: return PowerPC::ppcState.fpscr;
	case 5: return PowerPC::ppcState.msr;
	case 6: return PowerPC::ppcState.spr[SPR_SRR0];
	case 7: return PowerPC::ppcState.spr[SPR_SRR1];
	case 8: return PowerPC::ppcState.Exceptions;
	case 9: return ProcessorInterface::GetMask();
	case 10: return ProcessorInterface::GetCause();
	case 11: return PowerPC::ppcState.spr[SPR_DSISR];
	case 12: return PowerPC::ppcState.spr[SPR_DAR];
	case 13: return (PowerPC::ppcState.pagetable_hashmask << 6) | PowerPC::ppcState.pagetable_base;
	default: return 0;
	}
}

static wxString GetValueByRowCol(int row, int col)
{
	if (row < 32)
	{
		switch (col)
		{
		case 0: return StrToWxStr(GekkoDisassembler::GetGPRName(row));
		case 1: return wxString::Format("%08x", GPR(row));
		case 2: return StrToWxStr(GekkoDisassembler::GetFPRName(row));
		case 3: return wxString::Format("%016llx", riPS0(row));
		case 4: return wxString::Format("%016llx", riPS1(row));
		case 5:
		{
			if (row < 4)
			{
				return wxString::Format("DBAT%01d", row);
			}
			if (row < 8)
			{
				return wxString::Format("IBAT%01d", row - 4);
			}
			if (row < 12)
			{
				return wxString::Format("DBAT%01d", row - 4);
			}
			if (row < 16)
			{
				return wxString::Format("IBAT%01d", row - 8);
			}
		}
		case 6:
		{
			if (row < 4)
			{
				return wxString::Format("%016llx", (u64)PowerPC::ppcState.spr[SPR_DBAT0U + row * 2] << 32 | PowerPC::ppcState.spr[SPR_DBAT0L + row * 2]);
			}
			if (row < 8)
			{
				return wxString::Format("%016llx", (u64)PowerPC::ppcState.spr[SPR_IBAT0U + (row - 4) * 2] << 32 | PowerPC::ppcState.spr[SPR_IBAT0L + (row - 4) * 2]);
			}
			if (row < 12)
			{
				return wxString::Format("%016llx", (u64)PowerPC::ppcState.spr[SPR_DBAT4U + (row - 12) * 2] << 32 | PowerPC::ppcState.spr[SPR_DBAT4L + (row - 12) * 2]);
			}
			if (row < 16)
			{
				return wxString::Format("%016llx", (u64)PowerPC::ppcState.spr[SPR_IBAT4U + (row - 16) * 2] << 32 | PowerPC::ppcState.spr[SPR_IBAT4L + (row - 16) * 2]);
			}
		}
		case 7:
		{
			if (row < 16)
			{
				return wxString::Format("SR%02d", row);
			}
		}
		case 8:
		{
			if (row < 16)
			{
				return wxString::Format("%08x", PowerPC::ppcState.sr[row]);
			}
		}
		default: return wxEmptyString;
		}
	}
	else
	{
		if (row - 32 < NUM_SPECIALS)
		{
			switch (col)
			{
			case 0: return StrToWxStr(special_reg_names[row - 32]);
			case 1: return wxString::Format("%08x", GetSpecialRegValue(row - 32));
			default: return wxEmptyString;
			}
		}
	}
	return wxEmptyString;
}

wxString CRegTable::GetValue(int row, int col)
{
	return GetValueByRowCol(row, col);
}

static void SetSpecialRegValue(int reg, u32 value)
{
	switch (reg)
	{
	case 0: PowerPC::ppcState.pc = value; break;
	case 1: PowerPC::ppcState.spr[SPR_LR] = value; break;
	case 2: PowerPC::ppcState.spr[SPR_CTR] = value; break;
	case 3: SetCR(value); break;
	case 4: PowerPC::ppcState.fpscr = value; break;
	case 5: PowerPC::ppcState.msr = value; break;
	case 6: PowerPC::ppcState.spr[SPR_SRR0] = value; break;
	case 7: PowerPC::ppcState.spr[SPR_SRR1] = value; break;
	case 8: PowerPC::ppcState.Exceptions = value; break;
	// Should we just change the value, or use ProcessorInterface::SetInterrupt() to make the system aware?
	// case 9: return ProcessorInterface::GetMask();
	// case 10: return ProcessorInterface::GetCause();
	case 11: PowerPC::ppcState.spr[SPR_DSISR] = value; break;
	case 12: PowerPC::ppcState.spr[SPR_DAR] = value; break;
	//case 13: (PowerPC::ppcState.pagetable_hashmask << 6) | PowerPC::ppcState.pagetable_base;
	default: return;
	}
}

void CRegTable::SetValue(int row, int col, const wxString& strNewVal)
{
	u32 newVal = 0;
	if (TryParse("0x" + WxStrToStr(strNewVal), &newVal))
	{
		if (row < 32)
		{
			if (col == 1)
				GPR(row) = newVal;
			else if (col == 3)
				riPS0(row) = newVal;
			else if (col == 4)
				riPS1(row) = newVal;
		}
		else
		{
			if ((row - 32 < NUM_SPECIALS) && (col == 1))
			{
				SetSpecialRegValue(row - 32, newVal);
			}
		}
	}
}

void CRegTable::UpdateCachedRegs()
{
	for (int i = 0; i < 32; ++i)
	{
		m_CachedRegHasChanged[i] = (m_CachedRegs[i] != GPR(i));
		m_CachedRegs[i] = GPR(i);

		m_CachedFRegHasChanged[i][0] = (m_CachedFRegs[i][0] != riPS0(i));
		m_CachedFRegs[i][0] = riPS0(i);
		m_CachedFRegHasChanged[i][1] = (m_CachedFRegs[i][1] != riPS1(i));
		m_CachedFRegs[i][1] = riPS1(i);
	}
	for (int i = 0; i < NUM_SPECIALS; ++i)
	{
		m_CachedSpecialRegHasChanged[i] = (m_CachedSpecialRegs[i] != GetSpecialRegValue(i));
		m_CachedSpecialRegs[i] = GetSpecialRegValue(i);
	}
}

wxGridCellAttr *CRegTable::GetAttr(int row, int col, wxGridCellAttr::wxAttrKind)
{
	wxGridCellAttr *attr = new wxGridCellAttr();

	attr->SetBackgroundColour(*wxWHITE);
	attr->SetFont(DebuggerFont);

	switch (col)
	{
	case 1:
		attr->SetAlignment(wxALIGN_CENTER, wxALIGN_CENTER);
		break;
	case 3:
	case 4:
		attr->SetAlignment(wxALIGN_RIGHT, wxALIGN_CENTER);
		break;
	default:
		attr->SetAlignment(wxALIGN_LEFT, wxALIGN_CENTER);
		break;
	}

	bool red = false;
	switch (col)
	{
	case 1: red = row < 32 ? m_CachedRegHasChanged[row] : m_CachedSpecialRegHasChanged[row-32]; break;
	case 3:
	case 4: red = row < 32 ? m_CachedFRegHasChanged[row][col-3] : false; break;
	}

	attr->SetTextColour(red ? *wxRED : *wxBLACK);
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

	Bind(wxEVT_GRID_CELL_RIGHT_CLICK, &CRegisterView::OnMouseDownR, this);
	Bind(wxEVT_MENU, &CRegisterView::OnPopupMenu, this);

	AutoSizeColumns();
}

void CRegisterView::Update()
{
	ForceRefresh();
	((CRegTable *)GetTable())->UpdateCachedRegs();
}

void CRegisterView::OnMouseDownR(wxGridEvent& event)
{
	// popup menu
	int row = event.GetRow();
	int col = event.GetCol();

	wxString strNewVal = GetValueByRowCol(row, col);
	TryParse("0x" + WxStrToStr(strNewVal), &m_selectedAddress);

	wxMenu menu;
	menu.Append(IDM_WATCHADDRESS, _("Add to &watch"));
	menu.Append(IDM_VIEWMEMORY, _("View &memory"));
	PopupMenu(&menu);
}

void CRegisterView::OnPopupMenu(wxCommandEvent& event)
{
	CFrame* main_frame = (CFrame*)(GetParent()->GetParent());
	CCodeWindow* code_window = main_frame->g_pCodeWindow;
	CWatchWindow* watch_window = code_window->m_WatchWindow;
	CMemoryWindow* memory_window = code_window->m_MemoryWindow;

	switch (event.GetId())
	{
	case IDM_WATCHADDRESS:
		PowerPC::watches.Add(m_selectedAddress);
		if (watch_window)
			watch_window->NotifyUpdate();
		Refresh();
		break;
	case IDM_VIEWMEMORY:
		if (memory_window)
			memory_window->JumpToAddress(m_selectedAddress);
		Refresh();
		break;
	}
	event.Skip();
}
