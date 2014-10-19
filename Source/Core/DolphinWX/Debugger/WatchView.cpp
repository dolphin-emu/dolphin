// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <wx/chartype.h>
#include <wx/colour.h>
#include <wx/defs.h>
#include <wx/grid.h>
#include <wx/string.h>
#include <wx/windowid.h>

#include "Common/GekkoDisassembler.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/DebuggerUIUtil.h"
#include "DolphinWX/Debugger/WatchView.h"

class wxWindow;

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

static void SetWatchName(int count, std::string value)
{
	PowerPC::watches.UpdateName(count - 1, value);
}

static void SetWatchValue(int count, u32 value)
{
	Memory::WriteUnchecked_U32(value, GetWatchAddr(count));
}

wxString CWatchTable::GetValue(int row, int col)
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
	else if (row <= PowerPC::watches.GetWatches().size())
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
					return  Memory::GetString(addr, 32).c_str();
				else
					return wxEmptyString;
			}
			default: return wxEmptyString;
			}
		}
	}
	return wxEmptyString;
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

wxGridCellAttr *CWatchTable::GetAttr(int row, int col, wxGridCellAttr::wxAttrKind)
{
	wxGridCellAttr *attr = new wxGridCellAttr();

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
		switch (col)
		{
		case 1: red = m_CachedWatchHasChanged[row]; break;
		}

		attr->SetTextColour(red ? *wxRED : *wxBLACK);

		if (row > (int)(PowerPC::watches.GetWatches().size() + 1))
		{
			attr->SetReadOnly(true);
			attr->SetBackgroundColour(*wxLIGHT_GREY);
		}
	}
	attr->IncRef();
	return attr;
}

CWatchView::CWatchView(wxWindow *parent, wxWindowID id)
	: wxGrid(parent, id)
{
	SetTable(new CWatchTable(), false);
	SetRowLabelSize(0);
	SetColLabelSize(0);
	DisableDragRowSize();

	if (PowerPC::GetState() != PowerPC::CPU_POWERDOWN)
	{
		AutoSizeColumns();
	}
}

void CWatchView::Update()
{
	if (PowerPC::GetState() != PowerPC::CPU_POWERDOWN)
	{
		ForceRefresh();
		((CWatchTable *)GetTable())->UpdateWatch();
	}
}
