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

#include "PBView.h"

#include <iostream>
#include <string>
#include <stdio.h>
#include <stdlib.h>


// external declarations
extern const char* GetGRPName(unsigned int index);


CPBView::CPBView(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
	: wxListCtrl(parent, id, pos, size, style)
{
}

void CPBView::Update()
{
	ClearAll();

	InsertColumn(1, wxT("upd4"), wxLIST_FORMAT_LEFT, 90);
	InsertColumn(1, wxT("upd3"), wxLIST_FORMAT_LEFT, 90);
	InsertColumn(1, wxT("upd2"), wxLIST_FORMAT_LEFT, 90);
	InsertColumn(1, wxT("upd1"), wxLIST_FORMAT_LEFT, 90);
	InsertColumn(1, wxT("upd0"), wxLIST_FORMAT_LEFT, 90);

	InsertColumn(1, wxT("r_lo"), wxLIST_FORMAT_LEFT, 90);
	InsertColumn(1, wxT("r_hi"), wxLIST_FORMAT_LEFT, 90);
	InsertColumn(1, wxT("ratio"), wxLIST_FORMAT_LEFT, 90);

	InsertColumn(1, wxT("frac"), wxLIST_FORMAT_LEFT, 90);

	InsertColumn(1, wxT("coef"), wxLIST_FORMAT_LEFT, 90);
	InsertColumn(1, wxT("src_t"), wxLIST_FORMAT_LEFT, 90);
	InsertColumn(1, wxT("form"), wxLIST_FORMAT_LEFT, 90);

	InsertColumn(1, wxT("isstr"), wxLIST_FORMAT_LEFT, 90);

	InsertColumn(1, wxT("yn2"), wxLIST_FORMAT_LEFT, 90);
	InsertColumn(1, wxT("yn1"), wxLIST_FORMAT_LEFT, 90);
	InsertColumn(1, wxT("pred_s"), wxLIST_FORMAT_LEFT, 90);
	InsertColumn(1, wxT("isloop"), wxLIST_FORMAT_LEFT, 90);
	InsertColumn(1, wxT("volr"), wxLIST_FORMAT_LEFT, 90);
	InsertColumn(1, wxT("voll"), wxLIST_FORMAT_LEFT, 90);
	InsertColumn(1, wxT("loopto"), wxLIST_FORMAT_LEFT, 90);
	InsertColumn(1, wxT("end"), wxLIST_FORMAT_LEFT, 90);
	InsertColumn(0, wxT("pos"), wxLIST_FORMAT_LEFT, 90);
	InsertColumn(0, wxT("run"), wxLIST_FORMAT_RIGHT, 50);
	InsertColumn(0, wxT("Block"), wxLIST_FORMAT_CENTER, 40);

	for (int i = 0; i < 64; i++)
	{
		InsertItemInReportView(i);
	}
}

void CPBView::InsertItemInReportView(int _Row)
{
	long tmp = InsertItem(_Row, wxString::Format(wxT("%02i"), _Row), 0);
	SetItemData(tmp, _Row);

	wxString text;

	// A somewhat primitive attempt to show the playing history for a certain block.
	char cbuff [33];

	sprintf(cbuff, "%08i", m_CachedRegs[_Row][0]); //TODO?
	std::string c = cbuff;
	int n[8];

	for (int j = 0; j < 8; j++)
	{
		n[j] = atoi( c.substr(j, 1).c_str() );
		// 149 = dot, 160 = space
		if (n[j] == 1)
			n[j] = 149;
		else
			n[j] = 160;
	}
	// pretty neat huh?
	SetItem(tmp, 1, wxString::Format(wxT("%c%c%c%c%c%c%c%c"), n[0],n[1],n[2],n[3],n[4],n[5],n[6],n[7]));

	for (int column = 2; column < GetColumnCount(); column++)
		SetItem(tmp, column, wxString::Format(wxT("0x%08x"), m_CachedRegs[_Row][column]));
}
