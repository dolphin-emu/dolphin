// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#define wxUSE_XPM_IN_MSW 1
#define USE_XPM_BITMAPS 1

#include <memory>
#include <vector>

#include <wx/control.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/graphics.h>
#include <wx/windowid.h>

#include "Common/CommonTypes.h"

wxDECLARE_EVENT(wxEVT_CODEVIEW_CHANGE, wxCommandEvent);

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
	void OnScrollWheel(wxMouseEvent& event);
	void OnMouseDown(wxMouseEvent& event);
	void OnMouseMove(wxMouseEvent& event);
	void OnMouseUpL(wxMouseEvent& event);
	void OnMouseUpR(wxMouseEvent& event);
	void OnPopupMenu(wxCommandEvent& event);
	void InsertBlrNop(int);

	void ToggleBreakpoint(u32 address);

	u32 GetSelection()
	{
		return m_selection;
	}

	void Center(u32 addr)
	{
		m_curAddress = addr;
		m_selection = addr;
		Refresh();
	}

	void SetPlain()
	{
		m_plain = true;
	}

private:
	void RaiseEvent();
	int YToAddress(int y);

	u32 AddrToBranch(u32 addr);
	void OnResize(wxSizeEvent& event);

	void MoveTo(int x, int y)
	{
		m_lx = x;
		m_ly = y;
	}

	void LineTo(std::unique_ptr<wxGraphicsContext>& dc, int x, int y);

	struct BlrStruct // for IDM_INSERTBLR
	{
		u32 address;
		u32 oldValue;
	};
	std::vector<BlrStruct> m_blrList;

	DebugInterface* m_debugger;
	SymbolDB* m_symbol_db;

	bool m_plain;

	int m_curAddress;
	int m_align;
	int m_rowHeight;

	u32 m_selection;
	u32 m_oldSelection;
	bool m_selecting;

	int m_lx, m_ly;
};
