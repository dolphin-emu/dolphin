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
#include "Common.h"

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
};


BEGIN_EVENT_TABLE(CMemoryView, wxControl)
EVT_ERASE_BACKGROUND(CMemoryView::OnErase)
EVT_PAINT(CMemoryView::OnPaint)
EVT_LEFT_DOWN(CMemoryView::OnMouseDown)
EVT_LEFT_UP(CMemoryView::OnMouseUpL)
EVT_MOTION(CMemoryView::OnMouseMove)
EVT_RIGHT_DOWN(CMemoryView::OnMouseDown)
EVT_RIGHT_UP(CMemoryView::OnMouseUpR)
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
      showHex(false)
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


void CMemoryView::OnMouseDown(wxMouseEvent& event)
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
		debugger->toggleBreakpoint(YToAddress(y));
		redraw();
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
				OnMouseDown(event);
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
		    sprintf(temp, "%08x", debugger->readMemory(selection));
		    wxTheClipboard->SetData(new wxTextDataObject(wxString::FromAscii(temp)));
	    }
		    break;
#endif
	}

#if wxUSE_CLIPBOARD
	wxTheClipboard->Close();
#endif
	event.Skip(true);
}


void CMemoryView::OnMouseUpR(wxMouseEvent& event)
{
	// popup menu
	wxMenu menu;
	//menu.Append(IDM_GOTOINMEMVIEW, "&Goto in mem view");
#if wxUSE_CLIPBOARD
	menu.Append(IDM_COPYADDRESS, wxString::FromAscii("Copy &address"));
	menu.Append(IDM_COPYHEX, wxString::FromAscii("Copy &hex"));
#endif
	PopupMenu(&menu);
	event.Skip(true);
}


void CMemoryView::OnErase(wxEraseEvent& event)
{}


void CMemoryView::OnPaint(wxPaintEvent& event)
{
	wxPaintDC dc(this);
	int fontSize = 9;
	wxRect rc = GetClientRect();
	dc.SetFont(DefaultFont);
	struct branch
	{
		int src, dst, srcAddr;
	};

	branch branches[256];
	int numBranches = 0;
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
	wxBrush bpBrush(_T("#FF3311")); // red
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
		debugger->getRawMemoryString(address, mem, 256);
		dc.SetTextForeground(_T("#000080"));
		dc.DrawText(wxString::FromAscii(mem), 17+fontSize*(8), rowY1);
		dc.SetTextForeground(_T("#000000"));

		if (debugger->isAlive())
		{
			char dis[256] = {0};
			u32 mem = debugger->readMemory(address);
			float flt = *(float *)(&mem);
			sprintf(dis, "f: %f", flt);
			char desc[256] = "";

			dc.DrawText(wxString::FromAscii(dis), 17+fontSize*(8+8), rowY1);

			if (desc[0] == 0)
			{
				strcpy(desc, debugger->getDescription(address).c_str());
			}

			dc.SetTextForeground(_T("#0000FF"));

			//char temp[256];
			//UnDecorateSymbolName(desc,temp,255,UNDNAME_COMPLETE);
			if (strlen(desc))
			{
				dc.DrawText(wxString::FromAscii(desc), 17+fontSize*(8+8+8+30), rowY1);
			}

			if (debugger->isBreakpoint(address))
			{
				dc.SetBrush(bpBrush);
				dc.DrawRectangle(2, rowY1 + 1, 11, 11);
//				DrawIconEx(hdc, 2, rowY1, breakPoint, 32, 32, 0, 0, DI_NORMAL);
			}
		}
	}

	dc.SetPen(currentPen);
	/*
	   for (i = 0; i < numBranches; i++)
	   {
	    int x = 250 + (branches[i].srcAddr % 9) * 8;
	    MoveToEx(hdc, x-2, branches[i].src, 0);

	    if (branches[i].dst < rect.bottom + 200 && branches[i].dst > -200)
	    {
	    LineTo(hdc, x+2, branches[i].src);
	    LineTo(hdc, x+2, branches[i].dst);
	    LineTo(hdc, x-4, branches[i].dst);

	    MoveToEx(hdc, x, branches[i].dst - 4,0);
	    LineTo(hdc, x-4, branches[i].dst);
	    LineTo(hdc, x+1, branches[i].dst+5);
	    }
	    else
	    {
	    LineTo(hdc, x+4, branches[i].src);
	    //MoveToEx(hdc,x+2,branches[i].dst-4,0);
	    //LineTo(hdc,x+6,branches[i].dst);
	    //LineTo(hdc,x+1,branches[i].dst+5);
	    }
	    //LineTo(hdc,x,branches[i].dst+4);

	    //LineTo(hdc,x-2,branches[i].dst);
	   }*/
}


