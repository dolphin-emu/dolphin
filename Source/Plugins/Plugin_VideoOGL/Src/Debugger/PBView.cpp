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


// ---------------------------------------------------------------------------------------
// includes
// -----------------
#include "Globals.h"
#include "PBView.h"

#include <iostream>
#include <string>
#include <stdio.h>
#include <stdlib.h>



// ---------------------------------------------------------------------------------------
// external declarations
// -----------------
extern const char* GetGRPName(unsigned int index);


// ---------------------------------------------------------------------------------------
// No buttons or events so far
// -----------------
BEGIN_EVENT_TABLE(CPBView, wxListCtrl)
END_EVENT_TABLE()


// =======================================================================================
// The main wxListCtrl
// -------------
CPBView::CPBView(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
	: wxListCtrl(parent, id, pos, size, style)
{
	InsertColumn(0, wxT("Block"), wxLIST_FORMAT_CENTER, 40);

	SetFont(wxFont(8, wxSWISS, wxNORMAL, wxNORMAL, false, wxT("Segoe UI")));

	for (int i = 0; i < 1; i++)
	{

		// Print values from 0 to 63
		char buffer [33];
		sprintf(buffer, "%02i", i);
		int Item = InsertItem(0, wxString::FromAscii(buffer));


		wxListItem item;
		item.SetId(Item);
		item.SetBackgroundColour(0xFFFFFF);
		item.SetData(i);
		SetItem(item);
	}

	// This is a wx call that leads to MSWDrawSubItem
	Refresh();
}


void
CPBView::Update()
{

	Refresh();
	
}


bool
CPBView::MSWDrawSubItem(wxPaintDC& rPainDC, int item, int subitem)
{
	bool Result = false;

	// don't change 0, it has the block values
	if(subitem > 0)
	{
	#ifdef __WXMSW__ // what's this? should I use that?
	    const wxChar* bgColor = _T("#ffffff");
	    wxBrush bgBrush(bgColor);
	    wxPen bgPen(bgColor);

	    wxRect SubItemRect;
	    this->GetSubItemRect(item, subitem, SubItemRect);
	    rPainDC.SetBrush(bgBrush);
	    rPainDC.SetPen(bgPen);
	    rPainDC.DrawRectangle(SubItemRect);
	#endif
		// A somewhat primitive attempt to show the playing history for a certain block.

	    wxString text;
		if(subitem == 1)
		{
			char cbuff [33];

			sprintf(cbuff, "%08i", m_CachedRegs[subitem][item]);				
			std::string c = cbuff;
			int n[8];

			for (int j = 0; j < 8; j++)
			{	
				
				n[j] = atoi( c.substr(j, 1).c_str());
				// 149 = dot, 160 = space
				if (n[j] == 1){
					n[j] = 149;} else {n[j] = 160;}				
			}
			// pretty neat huh?
			text.Printf(wxT("%c%c%c%c%c%c%c%c"), n[0],n[1],n[2],n[3],n[4],n[5],n[6],n[7]);
			
		}
		else
		{
			text.Printf(wxT("0x%08x"), m_CachedRegs[subitem][item]);
		}
		#ifdef __WXMSW__
	    rPainDC.DrawText(text, SubItemRect.GetLeft() + 10, SubItemRect.GetTop() + 4);
	    #else
	    // May not show up pretty in !Win32
	    rPainDC.DrawText(text, 10, 4);
	    #endif

	    return(true);
	}
	else
	{
		// what does this mean?
		return(Result);
	}
}


