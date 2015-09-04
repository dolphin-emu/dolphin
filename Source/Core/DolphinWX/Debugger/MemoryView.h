// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/control.h>
#include "Common/CommonTypes.h"

class DebugInterface;

enum class MemoryDataType
{
	U8, U16, U32
};

class CMemoryView : public wxControl
{
public:
	CMemoryView(DebugInterface* debuginterface, wxWindow* parent);

	u32 GetSelection() const { return selection ; }
	int GetMemoryType() const { return memory; }

	void Center(u32 addr)
	{
		curAddress = addr;
		Refresh();
	}

	void SetDataType(MemoryDataType data_type)
	{
		dataType = data_type;
		Refresh();
	}

private:
	void OnPaint(wxPaintEvent& event);
	void OnMouseDownL(wxMouseEvent& event);
	void OnMouseMove(wxMouseEvent& event);
	void OnMouseUpL(wxMouseEvent& event);
	void OnMouseDownR(wxMouseEvent& event);
	void OnScrollWheel(wxMouseEvent& event);
	void OnPopupMenu(wxCommandEvent& event);

	int YToAddress(int y);
	void OnResize(wxSizeEvent& event);

	DebugInterface* debugger;

	int align;
	int rowHeight;

	u32 selection;
	u32 oldSelection;
	bool selecting;

	int memory;
	int curAddress;
	MemoryDataType dataType;

	enum EViewAsType
	{
		VIEWAS_ASCII = 0,
		VIEWAS_FP,
		VIEWAS_HEX,
	};

	EViewAsType viewAsType;
};
