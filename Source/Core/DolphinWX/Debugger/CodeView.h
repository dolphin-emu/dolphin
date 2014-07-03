// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#define wxUSE_XPM_IN_MSW 1
#define USE_XPM_BITMAPS 1

#include <vector>

#include <wx/control.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/windowid.h>

#include "Common/Common.h"

DECLARE_EVENT_TYPE(wxEVT_CODEVIEW_CHANGE, -1);

class DebugInterface;
class SymbolDB;
class wxPaintDC;
class wxWindow;

class CCodeView : public wxControl
{
public:
	CCodeView(DebugInterface* debuginterface, SymbolDB *symbol_db,
			wxWindow* parent, wxWindowID Id = wxID_ANY);
	void OnPaint(wxPaintEvent& event);
	void OnErase(wxEraseEvent& event);
	void OnMouseDown(wxMouseEvent& event);
	void OnMouseMove(wxMouseEvent& event);
	void OnMouseUpL(wxMouseEvent& event);
	void OnMouseUpR(wxMouseEvent& event);
	void OnPopupMenu(wxCommandEvent& event);
	void InsertBlrNop(int);

	u32 GetSelection() {return(selection);}
	void ToggleBreakpoint(u32 address);

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
		Refresh();
	}

	void SetPlain()
	{
		plain = true;
	}

private:
	void RaiseEvent();
	int YToAddress(int y);

	u32 AddrToBranch(u32 addr);
	void OnResize(wxSizeEvent& event);

	DebugInterface* debugger;
	SymbolDB* symbol_db;

	bool plain;

	int curAddress;
	int align;
	int rowHeight;

	u32 selection;
	u32 oldSelection;
	bool selecting;

	int lx, ly;
	void _MoveTo(int x, int y) {lx = x; ly = y;}
	void _LineTo(wxPaintDC &dc, int x, int y);

	DECLARE_EVENT_TABLE()
};
