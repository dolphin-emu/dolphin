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

#ifndef _MYLISTVIEW_H
#define _MYLISTVIEW_H

class CDisAsmListView
	: public CWindowImpl<CDisAsmListView, CListViewCtrl>,
	public CCustomDraw<CDisAsmListView>
{
	public:

		BEGIN_MSG_MAP(CDisAsmListView)
		CHAIN_MSG_MAP(CCustomDraw<CDisAsmListView>)
		END_MSG_MAP()

		DWORD OnPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/)
		{
			return(CDRF_NOTIFYITEMDRAW);
		}


		DWORD OnItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW lpNMCustomDraw)
		{
			NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(lpNMCustomDraw);

			// This is the prepaint stage for an item. Here's where we set the
			// item's text color. Our return value will tell Windows to draw the
			// item itself, but it will use the new color we set here for the background

			COLORREF crText;

			if ((pLVCD->nmcd.dwItemSpec % 2) == 0)
			{
				crText = RGB(200, 200, 255);
			}
			else
			{
				crText = RGB(255, 255, 255);
			}

			// Store the color back in the NMLVCUSTOMDRAW struct.
			pLVCD->clrTextBk = crText;

			// Tell Windows to paint the control itself.
			return(CDRF_DODEFAULT);
		}
};

#endif _MYLISTVIEW_H
