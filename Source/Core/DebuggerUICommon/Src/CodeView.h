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

#ifndef CODEVIEW_H_
#define CODEVIEW_H_

#define wxUSE_XPM_IN_MSW 1
#define USE_XPM_BITMAPS 1

#include <wx/wx.h>

// #include "Debugger.h"
#include "Common.h"

#include <vector>

DECLARE_EVENT_TYPE(wxEVT_CODEVIEW_CHANGE, -1);

class DebugInterface;
class SymbolDB;

class CCodeView : public wxControl
{
public:
	CCodeView(DebugInterface* debuginterface, SymbolDB *symbol_db, wxWindow* parent, wxWindowID Id = -1, const wxSize& Size = wxDefaultSize);
	wxSize DoGetBestSize() const;
	void OnPaint(wxPaintEvent& event);
	void OnErase(wxEraseEvent& event);
	void OnMouseDown(wxMouseEvent& event);
	void OnMouseMove(wxMouseEvent& event);
	void OnMouseUpL(wxMouseEvent& event);
	void OnMouseUpR(wxMouseEvent& event);
	void OnPopupMenu(wxCommandEvent& event);
	void InsertBlrNop(int);

	u32 GetSelection() {return(selection);}

	struct BlrStruct // for IDM_INSERTBLR
	{
		u32 Address;
		u32 OldValue;
	};
	std::vector<BlrStruct> BlrList;

	void Center(u32 addr)
	{
		curAddress = addr;
		selection = addr;
		redraw();
	}

	void SetPlain() {
		plain = true;
	}

private:
	void RaiseEvent();
	int YToAddress(int y);

	u32 AddrToBranch(u32 addr);

	void redraw() {Refresh();}

	DebugInterface* debugger;
	SymbolDB* symbol_db;

	bool plain;

	int curAddress;
	int align;
	int rowHeight;

	u32 selection;
	u32 oldSelection;
	bool selectionChanged;
	bool selecting;
	bool hasFocus;
	bool showHex;

	int lx, ly;
	void _MoveTo(int x, int y) {lx = x; ly = y;}
	void _LineTo(wxPaintDC &dc, int x, int y);

	DECLARE_EVENT_TABLE()
};

#endif /*CODEVIEW_H_*/
