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

#include "Common/GekkoDisassembler.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/BreakpointWindow.h"
#include "DolphinWX/Debugger/CodeWindow.h"
#include "DolphinWX/Debugger/DebuggerUIUtil.h"
#include "DolphinWX/Debugger/MemoryWindow.h"
#include "DolphinWX/Debugger/RegisterView.h"
#include "DolphinWX/Debugger/WatchView.h"
#include "DolphinWX/Debugger/WatchWindow.h"

class wxWindow;

enum
{
	IDM_DELETEWATCH,
	IDM_ADDMEMCHECK,
	IDM_VIEWMEMORY,
};


static std::string GetWatchName(int count)
{
	return PowerPC::watches.GetWatches().at(count - 1).name;
}

static u32 GetWatchAddr(int count)
{
	return PowerPC::watches.GetWatches().at(count - 1).iAddress;
}

static u32 GetWatchValue(int count)
{
	return Memory::ReadUnchecked_U32(GetWatchAddr(count));
}

static void AddWatchAddr(int count, u32 value)
{
	PowerPC::watches.Add(value);
}

static void UpdateWatchAddr(int count, u32 value)
{
	PowerPC::watches.Update(count - 1, value);
}

static void SetWatchName(int count, const std::string value)
{
	if ((count - 1) < (int)PowerPC::watches.GetWatches().size())
	{
		PowerPC::watches.UpdateName(count - 1, value);
	}
	else
	{
		PowerPC::watches.Add(0);
		PowerPC::watches.UpdateName(PowerPC::watches.GetWatches().size() - 1, value);
	}
}

static void SetWatchValue(int count, u32 value)
{
	Memory::WriteUnchecked_U32(value, GetWatchAddr(count));
}

static wxString GetValueByRowCol(int row, int col)
{
	if (row == 0)
	{
		// Column Labels
		switch (col)
		{
		case 0: return wxString::Format("Label");
		case 1: return wxString::Format("Addr");
		case 2: return wxString::Format("Hex");
		case 3: return wxString::Format("Dec");
		case 4: return wxString::Format("Str");
		default: return wxEmptyString;
		}
	}
	else if (row <= (int)PowerPC::watches.GetWatches().size())
	{
		if (PowerPC::GetState() != PowerPC::CPU_POWERDOWN)
		{
			switch (col)
			{
			case 0: return wxString::Format("%s", GetWatchName(row));
			case 1: return wxString::Format("%08x", GetWatchAddr(row));
			case 2: return wxString::Format("%08x", GetWatchValue(row));
			case 3: return wxString::Format("%lu", GetWatchValue(row));
			case 4:
			{
				u32 addr = GetWatchAddr(row);
				if (Memory::IsRAMAddress(addr))
					return Memory::GetString(addr, 32).c_str();
				else
					return wxEmptyString;
			}
			default: return wxEmptyString;
			}
		}
	}
	return wxEmptyString;
}

wxString CWatchTable::GetValue(int row, int col)
{
	return GetValueByRowCol(row, col);
}

void CWatchTable::SetValue(int row, int col, const wxString& strNewVal)
{
	u32 newVal = 0;
	if (col == 0 || TryParse("0x" + WxStrToStr(strNewVal), &newVal))
	{
		if (row > 0)
		{
			switch (col)
			{
			case 0:
			{
				SetWatchName(row, std::string(WxStrToStr(strNewVal)));
				break;
			}
			case 1:
			{
				if (row > (int)PowerPC::watches.GetWatches().size())
				{
					AddWatchAddr(row, newVal);
					row = (int)PowerPC::watches.GetWatches().size();
				}
				else
				{
					UpdateWatchAddr(row, newVal);
				}
				break;
			}
			case 2:
			{
				SetWatchValue(row, newVal);
				break;
			}
			default:
				break;
			}
		}
	}
}

void CWatchTable::UpdateWatch()
{
	for (int i = 0; i < (int)PowerPC::watches.GetWatches().size(); ++i)
	{
		m_CachedWatchHasChanged[i] = (m_CachedWatch[i] != GetWatchValue(i + 1));
		m_CachedWatch[i] = GetWatchValue(i + 1);
	}
}

wxGridCellAttr* CWatchTable::GetAttr(int row, int col, wxGridCellAttr::wxAttrKind)
{
	wxGridCellAttr* attr = new wxGridCellAttr();

	attr->SetBackgroundColour(*wxWHITE);
	attr->SetFont(DebuggerFont);

	switch (col)
	{
	case 1:
		attr->SetAlignment(wxALIGN_LEFT, wxALIGN_CENTER);
		break;
	case 3:
	case 4:
		attr->SetAlignment(wxALIGN_LEFT, wxALIGN_CENTER);
		break;
	default:
		attr->SetAlignment(wxALIGN_LEFT, wxALIGN_CENTER);
		break;
	}

	if (row == 0)
	{
		attr->SetReadOnly(true);
		attr->SetBackgroundColour(*wxBLACK);
		attr->SetTextColour(*wxWHITE);
	}
	else
	{
		bool red = false;
		if (col == 1)
			red = m_CachedWatchHasChanged[row];

		attr->SetTextColour(red ? *wxRED : *wxBLACK);

		if (row > (int)(PowerPC::watches.GetWatches().size() + 1) || (PowerPC::GetState() == PowerPC::CPU_POWERDOWN))
		{
			attr->SetReadOnly(true);
			attr->SetBackgroundColour(*wxLIGHT_GREY);
		}
	}
	attr->IncRef();
	return attr;
}

CWatchView::CWatchView(wxWindow* parent, wxWindowID id)
	: wxGrid(parent, id)
{
	SetTable(new CWatchTable(), false);
	SetRowLabelSize(0);
	SetColLabelSize(0);
	DisableDragRowSize();

	Bind(wxEVT_GRID_CELL_RIGHT_CLICK, &CWatchView::OnMouseDownR, this);
	Bind(wxEVT_MENU, &CWatchView::OnPopupMenu, this);
}

void CWatchView::Update()
{
	if (PowerPC::GetState() != PowerPC::CPU_POWERDOWN)
	{
		ForceRefresh();
		((CWatchTable *)GetTable())->UpdateWatch();
	}
}

void CWatchView::OnMouseDownR(wxGridEvent& event)
{
	// popup menu
	int row = event.GetRow();
	int col = event.GetCol();

	m_selectedRow = row;

	if (col == 1 || col == 2)
	{
		wxString strNewVal = GetValueByRowCol(row, col);
		TryParse("0x" + WxStrToStr(strNewVal), &m_selectedAddress);
	}

	wxMenu* menu = new wxMenu;
	if (row != 0 && row != (int)(PowerPC::watches.GetWatches().size() + 1))
		menu->Append(IDM_DELETEWATCH, _("&Delete watch"));

	if (row != 0 && row != (int)(PowerPC::watches.GetWatches().size() + 1) && (col == 1 || col == 2))
	{
#ifdef ENABLE_MEM_CHECK
		menu->Append(IDM_ADDMEMCHECK, _("Add memory &breakpoint"));
#endif
		menu->Append(IDM_VIEWMEMORY, _("View &memory"));
	}
	PopupMenu(menu);
}

void CWatchView::OnPopupMenu(wxCommandEvent& event)
{
	CFrame* main_frame = (CFrame*)(GetParent()->GetParent());
	CCodeWindow* code_window = main_frame->g_pCodeWindow;
	CWatchWindow* watch_window = code_window->m_WatchWindow;
	CMemoryWindow* memory_window = code_window->m_MemoryWindow;
	CBreakPointWindow* breakpoint_window = code_window->m_BreakpointWindow;

	wxString strNewVal;
	TMemCheck MemCheck;

	switch (event.GetId())
	{
	case IDM_DELETEWATCH:
		strNewVal = GetValueByRowCol(m_selectedRow, 1);
		if (TryParse("0x" + WxStrToStr(strNewVal), &m_selectedAddress))
		{
			PowerPC::watches.Remove(m_selectedAddress);
			if (watch_window)
				watch_window->NotifyUpdate();
			Refresh();
		}
		break;
	case IDM_ADDMEMCHECK:
		MemCheck.StartAddress = m_selectedAddress;
		MemCheck.EndAddress = m_selectedAddress;
		MemCheck.bRange = false;
		MemCheck.OnRead = true;
		MemCheck.OnWrite = true;
		MemCheck.Log = true;
		MemCheck.Break = true;
		PowerPC::memchecks.Add(MemCheck);

		if (breakpoint_window)
			breakpoint_window->NotifyUpdate();
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
