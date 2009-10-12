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

#include "DebuggerUIUtil.h"
#include "Common.h"
#include "Host.h"
#include "PowerPC/PowerPC.h"
#include "HW/Memmap.h"

#include "MemoryView.h"
#include <wx/event.h>
#include <wx/clipbrd.h>


enum
{
	IDM_GOTOINMEMVIEW = 12000,
	IDM_COPYADDRESS,
	IDM_COPYHEX,
	IDM_COPYCODE,
	IDM_RUNTOHERE,
	IDM_DYNARECRESULTS,
	IDM_TOGGLEMEMORY,
	IDM_VIEWASFP,
	IDM_VIEWASASCII,
};


BEGIN_EVENT_TABLE(CMemoryView, wxControl)
EVT_ERASE_BACKGROUND(CMemoryView::OnErase)
EVT_PAINT(CMemoryView::OnPaint)
EVT_LEFT_DOWN(CMemoryView::OnMouseDownL)
EVT_LEFT_UP(CMemoryView::OnMouseUpL)
EVT_MOTION(CMemoryView::OnMouseMove)
EVT_RIGHT_DOWN(CMemoryView::OnMouseDownR)
EVT_MENU(-1, CMemoryView::OnPopupMenu)
END_EVENT_TABLE()

CMemoryView::CMemoryView(DebugInterface* debuginterface, wxWindow* parent, wxWindowID Id, const wxSize& Size)
	: wxControl(parent, Id, wxDefaultPosition, Size),
      debugger(debuginterface),
      rowHeight(13),
      selection(0),
      oldSelection(0),
      selectionChanged(false),
      selecting(false),
      hasFocus(false),
      showHex(false),
	  memory(0),
	  viewAsType(VIEWAS_FP)
{
	rowHeight = 13;
	align = debuginterface->getInstructionSize(0);
	curAddress = debuginterface->getPC();
}


wxSize CMemoryView::DoGetBestSize() const
{
	wxSize bestSize;
	bestSize.x = 300;
	bestSize.y = 300;
	return(bestSize);
}


int CMemoryView::YToAddress(int y)
{
	wxRect rc = GetClientRect();
	int ydiff = y - rc.height / 2 - rowHeight / 2;
	ydiff = (int)(floorf((float)ydiff / (float)rowHeight)) + 1;
	return(curAddress + ydiff * align);
}


void CMemoryView::OnMouseDownL(wxMouseEvent& event)
{
	int x = event.m_x;
	int y = event.m_y;

	if (x > 16)
	{
		oldSelection = selection;
		selection = YToAddress(y);
		// SetCapture(wnd);
		bool oldselecting = selecting;
		selecting = true;

		if (!oldselecting || (selection != oldSelection))
		{
			redraw();
		}
	}
	else
	{
		int address = YToAddress(y);
		if (Memory::AreMemoryBreakpointsActivated() && !PowerPC::memchecks.GetMemCheck(address))
		{
			// Add Memory Check
			TMemCheck MemCheck;
			MemCheck.StartAddress = address;
			MemCheck.EndAddress = address;
			MemCheck.OnRead = true;
			MemCheck.OnWrite = true;

			MemCheck.Log = true;
			MemCheck.Break = true;

			PowerPC::memchecks.Add(MemCheck);

		}
		else
			PowerPC::memchecks.DeleteByAddress(address);

		redraw();
		Host_UpdateBreakPointView();
	}

	event.Skip(true);
}


void CMemoryView::OnMouseMove(wxMouseEvent& event)
{
	wxRect rc = GetClientRect();

	if (event.m_leftDown)
	{
		if (event.m_x > 16)
		{
			if (event.m_y < 0)
			{
				curAddress -= align;
				redraw();
			}
			else if (event.m_y > rc.height)
			{
				curAddress += align;
				redraw();
			}
			else
			{
				OnMouseDownL(event);
			}
		}
	}

	event.Skip(true);
}


void CMemoryView::OnMouseUpL(wxMouseEvent& event)
{
	if (event.m_x > 16)
	{
		curAddress = YToAddress(event.m_y);
		selecting = false;
		//ReleaseCapture();
		redraw();
	}

	event.Skip(true);
}


void CMemoryView::OnPopupMenu(wxCommandEvent& event)
{
#if wxUSE_CLIPBOARD
	wxTheClipboard->Open();
#endif

	switch (event.GetId())
	{
#if wxUSE_CLIPBOARD
	    case IDM_COPYADDRESS:
	    {
		    wxTheClipboard->SetData(new wxTextDataObject(wxString::Format(_T("%08x"), selection)));
	    }
	    break;

	    case IDM_COPYHEX:
	    {
		    char temp[24];
		    sprintf(temp, "%08x", debugger->readExtraMemory(memory, selection));
		    wxTheClipboard->SetData(new wxTextDataObject(wxString::FromAscii(temp)));
	    }
	    break;
#endif

		case IDM_TOGGLEMEMORY:
			memory ^= 1;
			redraw();
			break;

		case IDM_VIEWASFP:
			viewAsType = VIEWAS_FP;
			redraw();
			break;

		case IDM_VIEWASASCII:
			viewAsType = VIEWAS_ASCII;
			redraw();
			break;
	}

#if wxUSE_CLIPBOARD
	wxTheClipboard->Close();
#endif
	event.Skip(true);
}


void CMemoryView::OnMouseDownR(wxMouseEvent& event)
{
	// popup menu
	wxMenu menu;
	//menu.Append(IDM_GOTOINMEMVIEW, "&Goto in mem view");
#if wxUSE_CLIPBOARD
	menu.Append(IDM_COPYADDRESS, wxString::FromAscii("Copy &address"));
	menu.Append(IDM_COPYHEX, wxString::FromAscii("Copy &hex"));
#endif
	menu.Append(IDM_TOGGLEMEMORY, wxString::FromAscii("Toggle &memory (RAM/ARAM)"));

	wxMenu viewAsSubMenu;
	viewAsSubMenu.Append(IDM_VIEWASFP, wxString::FromAscii("FP value"));
	viewAsSubMenu.Append(IDM_VIEWASASCII, wxString::FromAscii("ASCII"));
	menu.AppendSubMenu(&viewAsSubMenu, wxString::FromAscii("View As:"));

	PopupMenu(&menu);

	event.Skip(true);
}


void CMemoryView::OnErase(wxEraseEvent& event)
{}


void CMemoryView::OnPaint(wxPaintEvent& event)
{
	wxPaintDC dc(this);
	wxRect rc = GetClientRect();
	dc.SetFont(DebuggerFont);
	int fontSize = DebuggerFont.GetPointSize();
	struct branch
	{
		int src, dst, srcAddr;
	};

	// TODO: Add any drawing code here...
	int width   = rc.width;
	int numRows = (rc.height / rowHeight) / 2 + 2;
	//numRows=(numRows&(~1)) + 1;
	dc.SetBackgroundMode(wxTRANSPARENT);
	const wxChar* bgColor = _T("#ffffff");
	wxPen nullPen(bgColor);
	wxPen currentPen(_T("#000000"));
	wxPen selPen(_T("#808080")); // gray
	nullPen.SetStyle(wxTRANSPARENT);

	wxBrush currentBrush(_T("#FFEfE8")); // light gray
	wxBrush pcBrush(_T("#70FF70")); // green
	wxBrush mcBrush(_T("#1133FF")); // blue
	wxBrush bgBrush(bgColor);
	wxBrush nullBrush(bgColor);
	nullBrush.SetStyle(wxTRANSPARENT);

	dc.SetPen(nullPen);
	dc.SetBrush(bgBrush);
	dc.DrawRectangle(0, 0, 16, rc.height);
	dc.DrawRectangle(0, 0, rc.width, 5+8);
	// TODO - clean up this freaking mess!!!!!
	int i;

	for (i = -numRows; i <= numRows; i++)
	{
		unsigned int address = curAddress + i * align;

		int rowY1 = rc.height / 2 + rowHeight * i - rowHeight / 2;
		int rowY2 = rc.height / 2 + rowHeight * i + rowHeight / 2;

		wxString temp = wxString::Format(_T("%08x"), address);
		u32 col = debugger->getColor(address);
		wxBrush rowBrush(wxColor(col >> 16, col >> 8, col));
		dc.SetBrush(nullBrush);
		dc.SetPen(nullPen);
		dc.DrawRectangle(0, rowY1, 16, rowY2);

		if (selecting && (address == selection))
		{
			dc.SetPen(selPen);
		}
		else
		{
			dc.SetPen(i == 0 ? currentPen : nullPen);
		}

		if (address == debugger->getPC())
		{
			dc.SetBrush(pcBrush);
		}
		else
		{
			dc.SetBrush(rowBrush);
		}

		dc.DrawRectangle(16, rowY1, width, rowY2 - 1);
		dc.SetBrush(currentBrush);
		dc.SetTextForeground(_T("#600000"));
		dc.DrawText(temp, 17, rowY1);
		char mem[256];
		debugger->getRawMemoryString(memory, address, mem, 256);
		dc.SetTextForeground(_T("#000080"));
		dc.DrawText(wxString::FromAscii(mem), 17+fontSize*(8), rowY1);
		dc.SetTextForeground(_T("#000000"));

		if (debugger->isAlive())
		{
			char dis[256] = {0};
			u32 mem_data = debugger->readExtraMemory(memory, address);

			if (viewAsType == VIEWAS_FP)
			{
				float flt = *(float *)(&mem_data);
				sprintf(dis, "f: %f", flt);
			}
			else if (viewAsType == VIEWAS_ASCII)
			{
				sprintf(dis, "%c%c%c%c",
					(mem_data&0xff000000)>>24, (mem_data&0xff0000)>>16,
					(mem_data&0xff00)>>8, mem_data&0xff);
			}
			else
				sprintf(dis, "INVALID VIEWAS TYPE");

			char desc[256] = "";

			dc.DrawText(wxString::FromAscii(dis), 77 + fontSize*(8 + 8), rowY1);

			if (desc[0] == 0)
			{
				strcpy(desc, debugger->getDescription(address).c_str());
			}

			dc.SetTextForeground(_T("#0000FF"));

			if (strlen(desc))
			{
				dc.DrawText(wxString::FromAscii(desc), 17+fontSize*(8+8+8+30), rowY1);
			}

			// Show blue memory check dot
			if (Memory::AreMemoryBreakpointsActivated() && PowerPC::memchecks.GetMemCheck(address))
			{
				dc.SetBrush(mcBrush);
				dc.DrawRectangle(2, rowY1 + 1, 11, 11);
			}
		}
	}

	dc.SetPen(currentPen);
}


