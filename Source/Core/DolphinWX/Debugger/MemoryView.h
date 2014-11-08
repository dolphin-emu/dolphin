// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <wx/control.h>
#include <wx/event.h>

#include "Common/CommonTypes.h"

class DebugInterface;
class wxWindow;

class CMemoryView : public wxControl
{
public:
	CMemoryView(DebugInterface* debuginterface, wxWindow* parent);
	void OnPaint(wxPaintEvent& event);
	void OnMouseDownL(wxMouseEvent& event);
	void OnMouseMove(wxMouseEvent& event);
	void OnMouseUpL(wxMouseEvent& event);
	void OnMouseDownR(wxMouseEvent& event);
	void OnScrollWheel(wxMouseEvent& event);
	void OnPopupMenu(wxCommandEvent& event);

	u32 GetSelection() { return selection ; }
	int GetMemoryType() { return memory; }

	void Center(u32 addr)
	{
		curAddress = addr;
		Refresh();
	}
	int dataType;   // u8,u16,u32
	int curAddress; // Will be accessed by parent

private:
	int YToAddress(int y);
	void OnResize(wxSizeEvent& event);

	DebugInterface* debugger;

	int align;
	int rowHeight;

	u32 selection;
	u32 oldSelection;
	bool selecting;

	int memory;

	enum EViewAsType
	{
		VIEWAS_ASCII = 0,
		VIEWAS_FP,
		VIEWAS_HEX,
	};

	EViewAsType viewAsType;
};
