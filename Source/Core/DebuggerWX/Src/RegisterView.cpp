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

extern const char* GetGRPName(unsigned int index);


BEGIN_EVENT_TABLE(CRegisterView, wxListCtrl)
END_EVENT_TABLE()

CRegisterView::CRegisterView(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
	: wxListCtrl(parent, id, pos, size, style)
{
	InsertColumn(1, wxT("Reg 16-31"), wxLIST_FORMAT_LEFT, 100);
	InsertColumn(0, wxT("Value"), wxLIST_FORMAT_CENTER, 80);
	InsertColumn(0, wxT("Reg 0-15"), wxLIST_FORMAT_LEFT, 100);
	InsertColumn(0, wxT("Value"), wxLIST_FORMAT_CENTER, 80);

	SetFont(wxFont(9, wxSWISS, wxNORMAL, wxNORMAL, false, wxT("Segoe UI")));

	for (int i = 0; i < 16; i++)
	{
		// 0-15
		int Item = InsertItem(0, wxString::FromAscii(GetGRPName(i)));

		// 16-31
		SetItem(Item, 2, wxString::FromAscii(GetGRPName(16 + i)));

		// just for easy sort

		wxListItem item;
		item.SetId(Item);
		item.SetBackgroundColour(0xFFFFFF);
		item.SetData(i);
		SetItem(item);
	}

	Refresh();
}


void
CRegisterView::Update()
{
	for (size_t i = 0; i < 16; i++)
	{
		// 0-15
		if (m_CachedRegs[i] != GPR(i))
		{
			m_CachedRegHasChanged[i] = true;
		}
		else
		{
			m_CachedRegHasChanged[i] = false;
		}

		m_CachedRegs[i] = GPR(i);

		// 16-31
		if (m_CachedRegs[16 + i] != GPR(16 + i))
		{
			m_CachedRegHasChanged[16 + i] = true;
		}
		else
		{
			m_CachedRegHasChanged[16 + i] = false;
		}

		m_CachedRegs[16 + i] = GPR(16 + i);
	}

	Refresh();
}

void CRegisterView::Refresh()
{
	for (size_t i = 0; i < 16; i++)
	{
		wxListItem item;
		item.SetId(i);
		item.SetColumn(1);
		char temp[16];
		sprintf(temp, "0x%08x",m_CachedRegs[i]);
		item.SetText(wxString::FromAscii(temp));
		SetItem(item);
	}
	for (size_t i = 0; i < 16; i++)
	{
		wxListItem item;
		item.SetId(i);
		item.SetColumn(3);
		char temp[16];
		sprintf(temp, "0x%08x",m_CachedRegs[16 + i]);
		item.SetText(wxString::FromAscii(temp));
		SetItem(item);
	}
}
#ifdef _WIN32
bool
CRegisterView::MSWDrawSubItem(wxPaintDC& rPainDC, int item, int subitem)
{
	bool Result = false;

#ifdef __WXMSW__
	switch (subitem)
	{
	    case 1:
	    case 3:
	    {
		    int Register = (subitem == 1) ? item : item + 16;

		    const wxChar* bgColor = _T("#ffffff");
		    wxBrush bgBrush(bgColor);
		    wxPen bgPen(bgColor);

		    wxRect SubItemRect;
		    this->GetSubItemRect(item, subitem, SubItemRect);
		    rPainDC.SetBrush(bgBrush);
		    rPainDC.SetPen(bgPen);
		    rPainDC.DrawRectangle(SubItemRect);

		    if (m_CachedRegHasChanged[Register])
		    {
			    rPainDC.SetTextForeground(_T("#FF0000"));
		    }
		    else
		    {
			    rPainDC.SetTextForeground(_T("#000000"));
		    }

		    wxString text;
		    text.Printf(wxT("0x%08x"), m_CachedRegs[Register]);
		    rPainDC.DrawText(text, SubItemRect.GetLeft() + 10, SubItemRect.GetTop() + 4);
		    return(true);
	    }
		    break;
	}
#endif
	return(Result);
}
#endif
